#include "NetBirdProvider.hpp"

#include "../../utils/Settings.hpp"
#include "../../vpn/VpnFileLogger.hpp"

#include <borealis/core/logger.hpp>

#include <algorithm>
#include <cstring>

#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
extern "C" {
#include <netbird.h>
}
#endif

using namespace brls::literals;

namespace {

// Moonlight's GameStream HTTP port. The TCP proxy is anchored here; the UDP
// relay covers the video/audio/control ports on its own.
constexpr uint16_t kGameStreamPort = 47989;

std::string vpn_log_path() {
    return Settings::instance().working_dir() + "/vpn.log";
}

void vpn_log(VpnFileLogger::Severity severity, const std::string& message) {
    VpnFileLogger::append(vpn_log_path(), "NetBird", severity, message);
}

} // namespace

std::string NetBirdProvider::id() const { return "netbird"; }
std::string NetBirdProvider::name() const { return "NetBird"; }

bool NetBirdProvider::available() const {
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    return true;
#else
    return false;
#endif
}

bool NetBirdProvider::start() {
#if !defined(__SWITCH__) || !defined(ENABLE_NETBIRD)
    lastError_ = "artemis/settings/netbird_unavailable";
    return false;
#else
    lastError_.clear();

    if (Settings::instance().remote_access_provider() !=
        RemoteAccessProviderId::NetBird) {
        vpn_log(VpnFileLogger::Severity::Warning,
                "start ignored because another provider is selected");
        return false;
    }

    const std::string server = Settings::instance().netbird_server();
    const std::string setupKey = Settings::instance().netbird_setup_key();
    if (server.empty()) {
        lastError_ = "artemis/settings/netbird_missing_server";
        return false;
    }
    if (setupKey.empty()) {
        lastError_ = "artemis/settings/netbird_missing_setup_key";
        return false;
    }

    // Never log the setup key itself.
    brls::Logger::info("NetBird: logging in to {}", server);
    vpn_log(VpnFileLogger::Severity::Info, "logging in to " + server);

    // netbird_init() performs the whole documented flow: setup-key login
    // against the management server, peer sync, relay connect, WireGuard key
    // derivation and handshake. Artemis must not reimplement any of that.
    char error[256]{};
    const int rc = netbird_init(server.c_str(), setupKey.c_str(), error,
                                sizeof(error));
    if (rc != 0) {
        lastError_ = error[0] ? error : "artemis/settings/netbird_login_failed";
        brls::Logger::warning("NetBird: login failed ({})", lastError_);
        vpn_log(VpnFileLogger::Severity::Error, "login failed: " + lastError_);
        return false;
    }

    started_ = true;
    brls::Logger::info("NetBird: connected, tunnel address {}", localAddress());
    vpn_log(VpnFileLogger::Severity::Info, "connected, tunnel address " + localAddress());
    return true;
#endif
}

void NetBirdProvider::poll() {
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    if (!started_) {
        return;
    }
    // Services lwIP timers, WireGuard keepalives and TCP retransmission.
    // Must run continuously (~every frame) while the provider is up.
    netbird_poll();
#endif
}

void NetBirdProvider::stop() {
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    if (!started_) {
        return;
    }
    vpn_log(VpnFileLogger::Severity::Info, "stopping transport");

    // Tear down in reverse order of setup so no worker thread is left holding
    // a socket that is about to be closed underneath it.
    netbird_proxy_stop_udp();
    netbird_proxy_stop();
    netbird_shutdown();

    activePeer_.clear();
    started_ = false;
#endif
}

std::string NetBirdProvider::status() const {
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    if (!lastError_.empty()) {
        return lastError_;
    }
    if (!started_) {
        return "artemis/settings/remote_access_off";
    }
    return netbird_is_ready() ? "artemis/settings/remote_access_connected"
                              : "artemis/settings/remote_access_connecting";
#else
    return "artemis/settings/netbird_unavailable";
#endif
}

std::string NetBirdProvider::lastError() const { return lastError_; }

std::string NetBirdProvider::localAddress() const {
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    if (!started_) {
        return {};
    }
    const char* ip = netbird_get_ip();
    return ip ? ip : std::string{};
#else
    return {};
#endif
}

std::vector<RemoteAccessPeer> NetBirdProvider::peers() const {
    std::vector<RemoteAccessPeer> out;
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    if (!started_ || !netbird_is_ready()) {
        return out;
    }

    const int count = netbird_get_peer_count();
    out.reserve(static_cast<size_t>(std::max(count, 0)));
    for (int i = 0; i < count; ++i) {
        char ip[64]{};
        char name[256]{};
        if (!netbird_get_peer(i, ip, sizeof(ip), name, sizeof(name))) {
            continue;
        }

        RemoteAccessPeer peer;
        peer.providerId = "netbird";
        peer.peerId = ip;
        peer.name = name[0] ? name : ip;
        // The real mesh address. Callers keep this as host identity and only
        // stream through 127.0.0.1 once a route is active.
        peer.address = ip;
        // Only advertise peers that actually answer on the GameStream port, so
        // a NAS or phone on the mesh does not show up as a streaming host.
        peer.online = netbird_peer_reachable(ip, kGameStreamPort, 400) == 1;
        out.push_back(std::move(peer));
    }
#endif
    return out;
}

bool NetBirdProvider::activateRoute(const std::string& peerId) {
#if !defined(__SWITCH__) || !defined(ENABLE_NETBIRD)
    (void)peerId;
    return false;
#else
    if (!started_ || peerId.empty()) {
        return false;
    }

    // Only ever route to an address the authenticated peer sync returned.
    // Without this check any 100.x address could redirect traffic into the
    // tunnel.
    bool known = false;
    const int count = netbird_get_peer_count();
    for (int i = 0; i < count && !known; ++i) {
        char ip[64]{};
        char name[256]{};
        if (netbird_get_peer(i, ip, sizeof(ip), name, sizeof(name))) {
            known = peerId == ip;
        }
    }
    if (!known) {
        brls::Logger::warning("NetBird: refusing route to unknown peer");
        vpn_log(VpnFileLogger::Severity::Warning, "refusing route to unknown peer");
        return false;
    }

    if (activePeer_ == peerId) {
        return true;
    }

    // One peer at a time: stop the previous route before starting the new one.
    netbird_proxy_stop_udp();
    netbird_proxy_stop();

    if (netbird_proxy_start(peerId.c_str(), kGameStreamPort) != 0) {
        lastError_ = "artemis/settings/netbird_proxy_failed";
        vpn_log(VpnFileLogger::Severity::Error, "TCP proxy start failed");
        return false;
    }
    if (netbird_proxy_start_udp(peerId.c_str()) != 0) {
        netbird_proxy_stop();
        lastError_ = "artemis/settings/netbird_proxy_failed";
        vpn_log(VpnFileLogger::Severity::Error, "UDP relay start failed");
        return false;
    }

    activePeer_ = peerId;
    vpn_log(VpnFileLogger::Severity::Info, "route active for peer " + peerId);
    return true;
#endif
}

void NetBirdProvider::deactivateRoute(const std::string& peerId) {
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    if (!started_ || activePeer_.empty() || activePeer_ != peerId) {
        return;
    }
    netbird_proxy_stop_udp();
    netbird_proxy_stop();
    activePeer_.clear();
    vpn_log(VpnFileLogger::Severity::Info, "route released");
#else
    (void)peerId;
#endif
}
