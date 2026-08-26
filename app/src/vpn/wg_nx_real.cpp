#include "wg_nx.h"

#include "SocketFdLock.hpp"
#include "WireGuardConfig.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cstdio>
#include <cstring>
#include <string>

// Real wg-nx backend for the Artemis WireGuard ABI.
//
// This shares the exact wg-nx/lwIP implementation that libnetbird.a is built
// on, so raw WireGuard and NetBird move packets through one tunnel stack
// instead of two divergent ones. The stub (wg_nx_stub.cpp) stays available for
// desktop/unit-test builds via -DARTEMIS_ALLOW_VPN_STUB=ON; a release Switch
// NRO always links this file.

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

} // namespace

struct WgNxTunnel {
    WireGuardConfig config;
    WgTunnel* tunnel = nullptr;
    bool running = false;
    // Filled from wg_get_ip() once connected so the UI reports the address the
    // tunnel actually negotiated, not the one the config asked for.
    std::string resolvedAddress;
};

extern "C" int wg_nx_is_real_backend(void) { return 1; }

extern "C" WgNxTunnel* wg_nx_tunnel_create(const char* conf_text) {
    if (!conf_text) {
        return nullptr;
    }

    auto* handle = new WgNxTunnel();
    handle->config = parse_wireguard_conf(conf_text);
    if (!handle->config.valid()) {
        wireguard_scrub(handle->config.privateKey);
        delete handle;
        return nullptr;
    }

    const auto& peer = handle->config.peers.front();

    WgConfig wg{};
    if (wg_key_from_base64(wg.private_key, handle->config.privateKey.c_str()) != 0) {
        wireguard_scrub(handle->config.privateKey);
        delete handle;
        return nullptr;
    }
    if (wg_key_from_base64(wg.peer_public_key, peer.publicKey.c_str()) != 0) {
        wireguard_scrub(handle->config.privateKey);
        delete handle;
        return nullptr;
    }
    if (!peer.presharedKey.empty()) {
        if (wg_key_from_base64(wg.preshared_key, peer.presharedKey.c_str()) != 0) {
            wireguard_scrub(handle->config.privateKey);
            delete handle;
            return nullptr;
        }
        wg.has_preshared_key = 1;
    }

    const std::string addressHost = address_host_part(handle->config.address);
    if (inet_pton(AF_INET, addressHost.c_str(), &wg.tunnel_ip) != 1) {
        wireguard_scrub(handle->config.privateKey);
        delete handle;
        return nullptr;
    }

    std::string endpointHost;
    uint16_t endpointPort = 0;
    if (!split_endpoint(peer.endpoint, endpointHost, endpointPort)) {
        wireguard_scrub(handle->config.privateKey);
        delete handle;
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
        delete handle;
        return nullptr;
    }
    return handle;
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

extern "C" void wg_nx_tunnel_stop(WgNxTunnel* handle) {
    if (!handle || !handle->tunnel || !handle->running) {
        return;
    }
    auto guard = SocketFdLock::instance().guard();
    wg_stop(handle->tunnel);
    handle->running = false;
}

extern "C" void wg_nx_tunnel_destroy(WgNxTunnel* handle) {
    if (!handle) {
        return;
    }
    wg_nx_tunnel_stop(handle);
    if (handle->tunnel) {
        auto guard = SocketFdLock::instance().guard();
        wg_close(handle->tunnel);
        handle->tunnel = nullptr;
    }
    delete handle;
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
