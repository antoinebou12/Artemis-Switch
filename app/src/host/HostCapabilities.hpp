#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace artemis::host {

enum class HostKind { Unknown, Sunshine, Apollo, Vibeshine, Punktfunk };

struct HostIdentity {
    HostKind kind = HostKind::Unknown;
    std::string product;
    std::string version;
    unsigned short webConsolePort = 47990;
};

struct HostCapabilities {
    HostKind kind = HostKind::Unknown;
    bool standardGameStream = true;
    bool extendedLaunchOptions = false;
    bool preciseRefreshRate = false;
    bool virtualDisplay = false;
    bool hostManagedVirtualDisplay = false;
    bool virtualDisplayDriverReady = false;
    bool serverCommands = false;
    bool clipboardSync = false;
    bool inputOnly = false;
    bool permissionAdvertised = false;
    uint32_t permissions = 0;
    std::vector<std::string> serverCommandList;
    std::string detectionReason;
};

struct HostMetadata {
    std::string appVersion;
    std::string gfeVersion;
    std::string gsVersion;
    std::string advertisedExtensions;
};

class HostCapabilityPolicy {
public:
    static HostCapabilities standardSunshine();
    static HostCapabilities apollo();
    static HostCapabilities vibeshine();
    static HostCapabilities punktfunk();
    static HostIdentity identityFor(HostKind kind);
    static HostCapabilities detect(const HostMetadata& metadata);
    static HostCapabilities fromServerInfo(
        HostKind identity,
        bool virtualDisplayCapable, bool virtualDisplayDriverReady,
        bool permissionAdvertised, uint32_t permissions,
        std::vector<std::string> serverCommands,
        bool currentAppHasUuid);
};

} // namespace artemis::host
