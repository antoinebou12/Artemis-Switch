#include "RemoteRouting.hpp"

#include "../features/host/HostAddressParse.hpp"
#include "../utils/Settings.hpp"
#include "../vpn/VpnFileLogger.hpp"
#include "RemoteAccessManager.hpp"
#include "providers/NetBirdProvider.hpp"

#include <borealis/core/logger.hpp>

#include <algorithm>

namespace artemis::remote {

RemoteRouteLease acquireRouteFor(const std::string& address) {
    if (address.empty()) {
        return {};
    }

    auto& manager = RemoteAccessManager::instance();
    const auto providerId = manager.activeProviderId();
    if (providerId.empty()) {
        return {};
    }

    auto* provider = manager.provider(providerId);
    if (!provider) {
        return {};
    }

    // Host addresses may carry an explicit ":port"; peer addresses never do.
    const auto parsed = artemis::host::parse_host_address(address);
    if (parsed.host.empty()) {
        return {};
    }

    // Only NetBird exposes a peer directory to route against. Raw WireGuard is a
    // routing VPN: its addresses are reachable directly once the tunnel is up,
    // so there is nothing to proxy.
    auto* netbird = dynamic_cast<NetBirdProvider*>(provider);
    if (!netbird || !netbird->isKnownPeer(parsed.host)) {
        return {};
    }

    RemoteRouteLease lease(manager, providerId, parsed.host, parsed.host,
                           kProxyAddress);
    if (!lease.isActive()) {
        brls::Logger::warning("Remote access: could not route to peer");
        return {};
    }

    brls::Logger::info("Remote access: routing {} through the tunnel", parsed.host);
    return lease;
}

std::string connectAddressFor(const RemoteRouteLease& lease,
                              const std::string& address) {
    if (!lease.isActive()) {
        return address;
    }

    // Preserve a non-default port: the proxy listens on the same port number it
    // forwards to, so "peer:47990" must become "127.0.0.1:47990".
    const auto parsed = artemis::host::parse_host_address(address);
    if (parsed.port.has_value()) {
        return std::string(kProxyAddress) + ":" + std::to_string(*parsed.port);
    }
    return kProxyAddress;
}

void logConnectionAttempt(const RemoteRouteLease& lease,
                           const std::string& requestedAddress,
                           const std::string& dialAddress) {
    if (!lease.isActive()) {
        return;
    }

    const std::string message = "GameStream route: requested=" +
                                requestedAddress + " effective=" +
                                dialAddress + " remote=" + lease.peerId();
    VpnFileLogger::append(Settings::instance().working_dir() + "/vpn.log",
                          "NetBird", VpnFileLogger::Severity::Info, message);
}

void logConnectionResult(const RemoteRouteLease& lease,
                         const std::string& dialAddress, bool succeeded,
                         const std::string& detail) {
    if (!lease.isActive()) {
        return;
    }

    std::string oneLineDetail = detail;
    std::replace(oneLineDetail.begin(), oneLineDetail.end(), '\n', ' ');
    std::replace(oneLineDetail.begin(), oneLineDetail.end(), '\r', ' ');

    std::string message = "GameStream handshake ";
    message += succeeded ? "succeeded" : "failed";
    message += " for peer " + lease.peerId() + " via " + dialAddress;
    if (!oneLineDetail.empty()) {
        message += ": " + oneLineDetail;
    }

    VpnFileLogger::append(
        Settings::instance().working_dir() + "/vpn.log", "NetBird",
        succeeded ? VpnFileLogger::Severity::Info
                  : VpnFileLogger::Severity::Error,
        message);
}

} // namespace artemis::remote
