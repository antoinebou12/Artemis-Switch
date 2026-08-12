#include "GameStreamHostCapabilities.hpp"

namespace artemis::host {

HostMetadata metadataFromServerData(const SERVER_DATA& server) {
    HostMetadata metadata;
    metadata.appVersion = server.serverInfoAppVersion;
    metadata.gfeVersion = server.serverInfoGfeVersion;
    metadata.gsVersion = server.gsVersion;
    return metadata;
}

HostCapabilities detectServerCapabilities(const SERVER_DATA& server) {
    const auto fromVersion =
        HostCapabilityPolicy::detect(metadataFromServerData(server));
    const auto fromFields = HostCapabilityPolicy::fromApolloServerInfo(
        server.virtualDisplayCapable, server.virtualDisplayDriverReady,
        server.hasApolloPermissionField, server.permission,
        server.serverCommands, !server.currentGameUuid.empty());

    const bool apolloIdentity = fromVersion.kind == HostKind::Apollo ||
                                fromFields.kind == HostKind::Apollo;

    // Sunshine without Apollo identity stays Sunshine-safe.
    if (!apolloIdentity) {
        if (server.isSunshine()) {
            auto capabilities = HostCapabilityPolicy::standardSunshine();
            capabilities.detectionReason = "SERVER_DATA identified Sunshine";
            return capabilities;
        }
        return fromVersion;
    }

    // Prefer field-derived capability flags when Apollo server-info fields exist;
    // otherwise use version/extension identity defaults.
    HostCapabilities capabilities =
        fromFields.kind == HostKind::Apollo ? fromFields : fromVersion;
    if (fromVersion.kind == HostKind::Apollo &&
        fromFields.kind == HostKind::Apollo) {
        capabilities.detectionReason =
            "Apollo identity with server-info fields";
    }
    return capabilities;
}

} // namespace artemis::host
