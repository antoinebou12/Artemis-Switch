#include "RemoteRouting.hpp"

#include "../features/host/HostAddressParse.hpp"
#include "../utils/Settings.hpp"
#include "../vpn/VpnFileLogger.hpp"
#include "RemoteAccessManager.hpp"

#include <borealis/core/logger.hpp>

#include <algorithm>

namespace {

std::string route_provider_label(const RemoteRouteLease& lease) {
    if (auto* provider =
            RemoteAccessManager::instance().provider(lease.providerId())) {
        return provider->name();
    }
    return lease.providerId().empty() ? "RemoteAccess" : lease.providerId();
}

} // namespace

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

    auto target = provider->resolveRoute(parsed.host);
    if (!target) {
        return {};
    }

    if (target->targetAddress.empty())
        target->targetAddress = parsed.host;
    if (target->connectAddress.empty())
        target->connectAddress = kProxyAddress;
    RemoteRouteLease lease(manager, providerId, std::move(*target));
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
    if (!lease.isActive())
        return;

    const std::string message = "GameStream route: requested=" +
                                requestedAddress + " effective=" +
                                dialAddress + " remote=" + lease.peerId();
    VpnFileLogger::append(Settings::instance().working_dir() + "/vpn.log",
                          route_provider_label(lease),
                          VpnFileLogger::Severity::Info, message);
}

void logConnectionResult(const RemoteRouteLease& lease,
                         const std::string& dialAddress, bool succeeded,
                         const std::string& detail) {
    if (!lease.isActive())
        return;

    std::string oneLineDetail = detail;
    std::replace(oneLineDetail.begin(), oneLineDetail.end(), '\n', ' ');
    std::replace(oneLineDetail.begin(), oneLineDetail.end(), '\r', ' ');

    std::string message = "GameStream handshake ";
    message += succeeded ? "succeeded" : "failed";
    message += " for peer " + lease.peerId() + " via " + dialAddress;
    if (!oneLineDetail.empty())
        message += ": " + oneLineDetail;

    VpnFileLogger::append(
        Settings::instance().working_dir() + "/vpn.log",
        route_provider_label(lease),
        succeeded ? VpnFileLogger::Severity::Info
                  : VpnFileLogger::Severity::Error,
        message);
}

} // namespace artemis::remote
