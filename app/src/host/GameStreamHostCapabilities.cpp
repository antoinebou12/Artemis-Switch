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
    auto capabilities = HostCapabilityPolicy::detect(metadataFromServerData(server));

    // SERVER_DATA::isSunshine() is authoritative for the standard fallback.
    // Do not promote an unidentified Sunshine server to Apollo.
    if (capabilities.kind != HostKind::Apollo && server.isSunshine()) {
        capabilities = HostCapabilityPolicy::standardSunshine();
        capabilities.detectionReason = "SERVER_DATA identified Sunshine";
    }

    return capabilities;
}

} // namespace artemis::host
