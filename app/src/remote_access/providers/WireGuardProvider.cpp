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

bool WireGuardProvider::canRouteAddress(const std::string& address) const {
#if defined(__SWITCH__) && defined(ENABLE_WIREGUARD)
    return WireGuardManager::instance().can_route_address(address);
#else
    (void)address;
    return false;
#endif
}

bool WireGuardProvider::activateRoute(const std::string& peerId) {
#if defined(__SWITCH__) && defined(ENABLE_WIREGUARD)
    return WireGuardManager::instance().activate_route(peerId);
#else
    (void)peerId;
    return false;
#endif
}

bool WireGuardProvider::prepareRouteForStreaming(const std::string& peerId) {
#if defined(__SWITCH__) && defined(ENABLE_WIREGUARD)
    return WireGuardManager::instance().prepare_route_for_streaming(peerId);
#else
    (void)peerId;
    return false;
#endif
}

void WireGuardProvider::deactivateRoute(const std::string& peerId) {
#if defined(__SWITCH__) && defined(ENABLE_WIREGUARD)
    WireGuardManager::instance().deactivate_route(peerId);
#else
    (void)peerId;
#endif
}
