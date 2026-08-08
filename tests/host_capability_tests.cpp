#include "HostCapabilities.hpp"

#include <cassert>

using namespace artemis::host;

int main() {
    const auto sunshine = HostCapabilityPolicy::standardSunshine();
    assert(sunshine.kind == HostKind::Sunshine);
    assert(sunshine.standardGameStream);
    assert(!sunshine.virtualDisplay);
    assert(!sunshine.serverCommands);
    assert(!sunshine.clipboardSync);
    assert(!sunshine.inputOnly);

    const auto apollo = HostCapabilityPolicy::apollo();
    assert(apollo.kind == HostKind::Apollo);
    assert(apollo.standardGameStream);
    assert(apollo.virtualDisplay);
    assert(apollo.serverCommands);
    assert(apollo.clipboardSync);
    assert(apollo.inputOnly);

    return 0;
}
