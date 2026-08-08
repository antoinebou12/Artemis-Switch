#include "HostCapabilities.hpp"

namespace artemis::host {

HostCapabilities HostCapabilityPolicy::standardSunshine() {
    HostCapabilities c;
    c.kind = HostKind::Sunshine;
    c.standardGameStream = true;
    return c;
}

HostCapabilities HostCapabilityPolicy::apollo() {
    HostCapabilities c;
    c.kind = HostKind::Apollo;
    c.standardGameStream = true;
    c.virtualDisplay = true;
    c.serverCommands = true;
    c.clipboardSync = true;
    c.inputOnly = true;
    return c;
}

} // namespace artemis::host
