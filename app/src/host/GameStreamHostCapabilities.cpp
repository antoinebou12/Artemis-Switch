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
    const HostKind identity = server.hostIdentity.kind != HostKind::Unknown
        ? server.hostIdentity.kind
        : fromVersion.kind;
    auto capabilities = HostCapabilityPolicy::fromServerInfo(
        identity,
        server.virtualDisplayCapable, server.virtualDisplayDriverReady,
        server.hasApolloPermissionField, server.permission,
        server.serverCommands, !server.currentGameUuid.empty());
    return capabilities;
}

} // namespace artemis::host
