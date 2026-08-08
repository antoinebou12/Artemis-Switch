#pragma once

namespace artemis::host {

enum class HostKind { Unknown, Sunshine, Apollo };

struct HostCapabilities {
    HostKind kind = HostKind::Unknown;
    bool standardGameStream = true;
    bool virtualDisplay = false;
    bool serverCommands = false;
    bool clipboardSync = false;
    bool inputOnly = false;
};

class HostCapabilityPolicy {
public:
    static HostCapabilities standardSunshine();
    static HostCapabilities apollo();
};

} // namespace artemis::host
