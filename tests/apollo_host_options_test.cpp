#include "../app/src/features/apollo/ApolloHostOptions.hpp"

#include <cassert>
#include <string>

using namespace artemis::apollo;

int main() {
    assert(!resolveVirtualDisplay({}, 1280, 720).enabled);
    ApolloHostOptions portrait;
    portrait.target = VirtualDisplayTarget::PortraitDocked;
    auto resolved = resolveVirtualDisplay(portrait, 1280, 720);
    assert(resolved.enabled && resolved.width == 1080 && resolved.height == 1920);

    ApolloHostOptions current;
    current.target = VirtualDisplayTarget::CurrentProfile;
    resolved = resolveVirtualDisplay(current, 720, 1280);
    assert(resolved.width == 720 && resolved.height == 1280);

    ApolloHostOptions huge;
    huge.target = VirtualDisplayTarget::Custom;
    huge.customWidth = 9000;
    huge.customHeight = 9000;
    resolved = resolveVirtualDisplay(huge, 0, 0);
    assert(static_cast<long long>(resolved.width) * resolved.height <= 1920LL * 1080LL);

    VirtualDisplayTarget parsed = VirtualDisplayTarget::Off;
    assert(virtualDisplayTargetFromName("portrait-handheld", parsed));
    assert(parsed == VirtualDisplayTarget::PortraitHandheld);
    assert(!virtualDisplayTargetFromName("apollo-shell", parsed));

    ApolloHostOptions custom;
    std::string error;
    assert(parseVirtualDisplaySpec("720x1280@60", custom, &error));
    assert(custom.target == VirtualDisplayTarget::Custom);
    assert(custom.customWidth == 720 && custom.customHeight == 1280);
    assert(custom.refreshRate == 60);
    assert(!parseVirtualDisplaySpec("1920x1920@60", custom, &error));
    assert(!error.empty());
    assert(!parseVirtualDisplaySpec("720x1280@144", custom));
    assert(!parseVirtualDisplaySpec("shell.exe", custom));
}
