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

std::string routeRefusalReason(const RemoteRouteLease& lease) {
    if (!lease.refused() || lease.providerId().empty())
        return {};
    if (auto* provider =
            RemoteAccessManager::instance().provider(lease.providerId())) {
        const std::string reason = provider->lastError();
        if (!reason.empty())
            return reason;
    }
    return "tunnel route refused for peer " + lease.peerId();
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
    if (!lease.isActive() && !lease.refused())
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

void logProxiedPairingResult(const std::string& dialAddress, bool succeeded,
                             const std::string& detail) {
    if (dialAddress != kProxyAddress)
        return;
    std::string oneLineDetail = detail;
    std::replace(oneLineDetail.begin(), oneLineDetail.end(), '\n', ' ');
    std::replace(oneLineDetail.begin(), oneLineDetail.end(), '\r', ' ');
    std::string message = std::string("Pairing through tunnel ") +
                          (succeeded ? "succeeded" : "failed");
    if (!oneLineDetail.empty())
        message += ": " + oneLineDetail;
    VpnFileLogger::append(Settings::instance().working_dir() + "/vpn.log",
                          "Remote",
                          succeeded ? VpnFileLogger::Severity::Info
                                    : VpnFileLogger::Severity::Error,
                          message);
}

void logHostSaveResult(const std::string& hostname, const std::string& address,
                       const std::string& mac, size_t hostsBefore,
                       size_t hostsAfter, bool foundAfterSave) {
    std::string message = "host save: name=" + hostname +
                          " address=" + address +
                          " mac=" + (mac.empty() ? std::string("none") : mac) +
                          " hosts " + std::to_string(hostsBefore) + " -> " +
                          std::to_string(hostsAfter);
    if (!foundAfterSave)
        message += " (NOT in the saved list)";
    else if (hostsAfter == hostsBefore)
        message += " (merged into an existing host with the same name/address)";
    else
        message += " (added)";
    VpnFileLogger::append(Settings::instance().working_dir() + "/vpn.log",
                          "Remote",
                          foundAfterSave ? VpnFileLogger::Severity::Info
                                         : VpnFileLogger::Severity::Error,
                          message);
}

} // namespace artemis::remote
