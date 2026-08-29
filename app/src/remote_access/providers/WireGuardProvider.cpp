#include "WireGuardProvider.hpp"
#include "../../vpn/WireGuardManager.hpp"
#include "../../vpn/WireGuardConfig.hpp"
#include "../../utils/Settings.hpp"
#include "../../remote_access/TunnelCore.hpp"

bool WireGuardProvider::available() const {
#if defined(__SWITCH__) && defined(ENABLE_WIREGUARD)
    return true;
#else
    return false;
#endif
}

bool WireGuardProvider::start() {
#if defined(__SWITCH__) && defined(ENABLE_WIREGUARD)
    if (Settings::instance().remote_access_provider() != RemoteAccessProviderId::WireGuard) return false;
    TunnelCore::instance().init();
    TunnelCore::instance().start();
    return WireGuardManager::instance().enable_from_settings();
#else
    return false;
#endif
}

void WireGuardProvider::stop() {
#if defined(__SWITCH__) && defined(ENABLE_WIREGUARD)
    WireGuardManager::instance().disable();
#endif
}

std::string WireGuardProvider::status() const {
#if defined(__SWITCH__) && defined(ENABLE_WIREGUARD)
    return WireGuardManager::instance().status_text();
#else
    return "Unavailable";
#endif
}

std::string WireGuardProvider::lastError() const {
#if defined(__SWITCH__) && defined(ENABLE_WIREGUARD)
    return WireGuardManager::instance().last_error();
#else
    return "";
#endif
}

std::string WireGuardProvider::localAddress() const {
#if defined(__SWITCH__) && defined(ENABLE_WIREGUARD)
    return WireGuardManager::instance().tunnel_address();
#else
    return "";
#endif
}

std::vector<RemoteAccessPeer> WireGuardProvider::peers() const {
#if defined(__SWITCH__) && defined(ENABLE_WIREGUARD)
    // WireGuard is a routing VPN, not a peer directory.
    // Do not synthesize GameStream hosts from AllowedIPs/Endpoint.
    // Returning empty prevents DiscoverManager from treating WireGuard peers as hosts.
    return {};
#else
    return {};
#endif
}

std::optional<RemoteRouteTarget>
WireGuardProvider::resolveRoute(std::string_view address) const {
#if defined(__SWITCH__) && defined(ENABLE_WIREGUARD)
    const std::string candidate(address);
    if (!WireGuardManager::instance().can_route_address(candidate))
        return std::nullopt;
    return RemoteRouteTarget{"wireguard:peer0", candidate, candidate,
                             "127.0.0.1", RemoteRouteMode::Proxy};
#else
    (void)address;
    return std::nullopt;
#endif
}

bool WireGuardProvider::activateRoute(const RemoteRouteTarget& target) {
#if defined(__SWITCH__) && defined(ENABLE_WIREGUARD)
    return WireGuardManager::instance().activate_route(target.peerAddress);
#else
    (void)target;
    return false;
#endif
}

bool WireGuardProvider::prepareRouteForStreaming(
    const RemoteRouteTarget& target) {
#if defined(__SWITCH__) && defined(ENABLE_WIREGUARD)
    return WireGuardManager::instance().prepare_route_for_streaming(
        target.peerAddress);
#else
    (void)target;
    return false;
#endif
}

void WireGuardProvider::deactivateRoute(const RemoteRouteTarget& target) {
#if defined(__SWITCH__) && defined(ENABLE_WIREGUARD)
    WireGuardManager::instance().deactivate_route(target.peerAddress);
#else
    (void)target;
#endif
}
