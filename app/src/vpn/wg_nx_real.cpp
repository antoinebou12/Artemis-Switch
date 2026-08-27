#include "wg_nx.h"

#include "SocketFdLock.hpp"
#include "WireGuardConfig.hpp"
#include <wg_lwip_relay.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

// Real wg-nx backend for the Artemis WireGuard ABI.
//
// This links the independently pinned top-level wg-nx archive. NetBird's copy
// is rewritten into a private symbol namespace during its staged build, so the
// two providers never share WireGuard, lwIP, callback, or timer state.

extern "C" {
#include <wireguard.h>
}

namespace {

// Address is stored as CIDR ("10.70.0.2/32"); wg-nx wants the bare host part.
std::string address_host_part(const std::string& address) {
    const auto slash = address.find('/');
    return slash == std::string::npos ? address : address.substr(0, slash);
}

// Endpoint is "host:port". IPv6 literals are not supported by wg-nx's
// WgConfig.endpoint_host, so a bracketed form is rejected rather than parsed
// into something that would silently connect to the wrong place.
bool split_endpoint(const std::string& endpoint, std::string& host, uint16_t& port) {
    if (endpoint.empty() || endpoint.front() == '[') {
        return false;
    }
    const auto colon = endpoint.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= endpoint.size()) {
        return false;
    }
    host = endpoint.substr(0, colon);
    const long parsed = std::strtol(endpoint.c_str() + colon + 1, nullptr, 10);
    if (parsed <= 0 || parsed > 65535) {
        return false;
    }
    port = static_cast<uint16_t>(parsed);
    return true;
}

constexpr std::array<std::uint16_t, 3> kTcpPorts{47989, 47984, 48010};
constexpr std::array<std::uint16_t, 5> kUdpPorts{47998, 48000, 47999, 48002,
                                                48010};

void relay_log(wgnx::LogLevel level, const char* message) {
    const char* label = level == wgnx::LogLevel::Error ? "ERROR" :
                        level == wgnx::LogLevel::Debug ? "DEBUG" : "INFO";
    std::fprintf(stderr, "[WIREGUARD-RELAY] %s %s\n", label,
                 message ? message : "");
}

} // namespace

struct WgNxTunnel {
    WireGuardConfig config;
    WgTunnel* tunnel = nullptr;
    bool running = false;
    // Filled from wg_get_ip() once connected so the UI reports the address the
    // tunnel actually negotiated, not the one the config asked for.
    std::string resolvedAddress;
    std::unique_ptr<wgnx::LwipRelay> relay;
    std::string activeTarget;
    bool udpPrepared = false;
};

namespace {

void clear_relay(WgNxTunnel* handle) {
    if (!handle)
        return;
    if (handle->relay) {
        auto guard = SocketFdLock::instance().guard();
        handle->relay.reset();
    }
    handle->activeTarget.clear();
    handle->udpPrepared = false;
}

} // namespace

extern "C" int wg_nx_is_real_backend(void) { return 1; }

extern "C" WgNxTunnel* wg_nx_tunnel_create(const char* conf_text) {
    if (!conf_text) {
        return nullptr;
    }

    auto handle = std::make_unique<WgNxTunnel>();
    handle->config = parse_wireguard_conf(conf_text);
    if (!handle->config.valid()) {
        wireguard_scrub(handle->config.privateKey);
        return nullptr;
    }

    const auto& peer = handle->config.peers.front();

    WgConfig wg{};
    if (wg_key_from_base64(wg.private_key, handle->config.privateKey.c_str()) != 0) {
        wireguard_scrub(handle->config.privateKey);
        return nullptr;
    }
    if (wg_key_from_base64(wg.peer_public_key, peer.publicKey.c_str()) != 0) {
        wireguard_scrub(handle->config.privateKey);
        return nullptr;
    }
    if (!peer.presharedKey.empty()) {
        if (wg_key_from_base64(wg.preshared_key, peer.presharedKey.c_str()) != 0) {
            wireguard_scrub(handle->config.privateKey);
            return nullptr;
        }
        wg.has_preshared_key = 1;
    }

    const std::string addressHost = address_host_part(handle->config.address);
    if (inet_pton(AF_INET, addressHost.c_str(), &wg.tunnel_ip) != 1) {
        wireguard_scrub(handle->config.privateKey);
        return nullptr;
    }

    std::string endpointHost;
    uint16_t endpointPort = 0;
    if (!split_endpoint(peer.endpoint, endpointHost, endpointPort)) {
        wireguard_scrub(handle->config.privateKey);
        return nullptr;
    }
    std::snprintf(wg.endpoint_host, sizeof(wg.endpoint_host), "%s", endpointHost.c_str());
    wg.endpoint_port = endpointPort;
    wg.keepalive_interval = static_cast<uint16_t>(
        peer.persistentKeepalive > 0 ? peer.persistentKeepalive : 25);

    {
        // wg_init opens the UDP socket; serialise it against the rest of the
        // app's socket teardown the same way the NetBird path does.
        auto guard = SocketFdLock::instance().guard();
        handle->tunnel = wg_init(&wg);
    }

    // The parsed secrets are now inside wg-nx; do not keep a second copy.
    std::memset(wg.private_key, 0, sizeof(wg.private_key));
    std::memset(wg.preshared_key, 0, sizeof(wg.preshared_key));
    wireguard_scrub(handle->config.privateKey);
    for (auto& configuredPeer : handle->config.peers) {
        wireguard_scrub(configuredPeer.presharedKey);
    }

    if (!handle->tunnel) {
        return nullptr;
    }
    return handle.release();
}

extern "C" int wg_nx_tunnel_start(WgNxTunnel* handle) {
    if (!handle || !handle->tunnel) {
        return -1;
    }
    if (handle->running) {
        return 0;
    }

    auto guard = SocketFdLock::instance().guard();
    if (wg_connect(handle->tunnel) != WG_OK) {
        return -1;
    }
    if (wg_start(handle->tunnel) != WG_OK) {
        wg_stop(handle->tunnel);
        return -1;
    }

    struct in_addr negotiated {};
    if (wg_get_ip(handle->tunnel, &negotiated) == WG_OK) {
        char text[INET_ADDRSTRLEN]{};
        if (inet_ntop(AF_INET, &negotiated, text, sizeof(text))) {
            handle->resolvedAddress = text;
        }
    }

    handle->running = true;
    return 0;
}

extern "C" int wg_nx_tunnel_can_route(WgNxTunnel* handle,
                                         const char* ipv4_address) {
    if (!handle || !handle->running || !ipv4_address ||
        handle->config.peers.size() != 1) {
        return 0;
    }
    return wireguard_ipv4_matches_allowed_ips(
               ipv4_address, handle->config.peers.front().allowedIps)
               ? 1
               : 0;
}

extern "C" int wg_nx_tunnel_activate_route(WgNxTunnel* handle,
                                              const char* ipv4_address) {
    if (!handle || !ipv4_address ||
        wg_nx_tunnel_can_route(handle, ipv4_address) != 1) {
        return -1;
    }
    if (handle->relay && handle->activeTarget == ipv4_address &&
        handle->relay->isRunning()) {
        return 0;
    }

    clear_relay(handle);

    wgnx::LwipRelayConfig relayConfig;
    relayConfig.log_callback = relay_log;
    auto relay = std::make_unique<wgnx::LwipRelay>(handle->tunnel, relayConfig);
    const std::string tunnelAddress =
        handle->resolvedAddress.empty()
            ? address_host_part(handle->config.address)
            : handle->resolvedAddress;

    {
        auto guard = SocketFdLock::instance().guard();
        if (!relay->start(tunnelAddress, ipv4_address)) {
            return -2;
        }
        for (const auto port : kTcpPorts) {
            if (relay->startTcpRelay(port, port) == 0) {
                relay.reset();
                return -3;
            }
        }
    }

    handle->relay = std::move(relay);
    handle->activeTarget = ipv4_address;
    handle->udpPrepared = false;
    return 0;
}

extern "C" int wg_nx_tunnel_prepare_stream(WgNxTunnel* handle,
                                              const char* ipv4_address) {
    if (!handle || !handle->relay || !handle->relay->isRunning() ||
        !ipv4_address || handle->activeTarget != ipv4_address) {
        return -1;
    }
    if (handle->udpPrepared)
        return 0;

    auto guard = SocketFdLock::instance().guard();
    for (const auto port : kUdpPorts) {
        if (handle->relay->startUdpRelay(port, port) == 0)
            return -2;
    }
    handle->udpPrepared = true;
    return 0;
}

extern "C" void wg_nx_tunnel_deactivate_route(WgNxTunnel* handle,
                                                 const char* ipv4_address) {
    if (!handle || !ipv4_address || handle->activeTarget != ipv4_address)
        return;
    clear_relay(handle);
}

extern "C" void wg_nx_tunnel_stop(WgNxTunnel* handle) {
    if (!handle || !handle->tunnel || !handle->running) {
        return;
    }
    clear_relay(handle);
    auto guard = SocketFdLock::instance().guard();
    wg_stop(handle->tunnel);
    handle->running = false;
}

extern "C" void wg_nx_tunnel_destroy(WgNxTunnel* handle) {
    if (!handle) {
        return;
    }
    std::unique_ptr<WgNxTunnel> owner(handle);
    wg_nx_tunnel_stop(owner.get());
    if (owner->tunnel) {
        auto guard = SocketFdLock::instance().guard();
        wg_close(owner->tunnel);
        owner->tunnel = nullptr;
    }
}

extern "C" const char* wg_nx_tunnel_address(WgNxTunnel* handle) {
    if (!handle) {
        return "";
    }
    if (!handle->resolvedAddress.empty()) {
        return handle->resolvedAddress.c_str();
    }
    return handle->config.address.c_str();
}
