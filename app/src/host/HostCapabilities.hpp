#pragma once

#include <string>

namespace artemis::host {

enum class HostKind { Unknown, Sunshine, Apollo };

struct HostCapabilities {
    HostKind kind = HostKind::Unknown;
    bool standardGameStream = true;
    bool virtualDisplay = false;
    bool serverCommands = false;
    bool clipboardSync = false;
    bool inputOnly = false;
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
    static HostCapabilities detect(const HostMetadata& metadata);
};

} // namespace artemis::host
