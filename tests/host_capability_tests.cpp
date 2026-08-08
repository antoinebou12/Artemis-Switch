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

    HostMetadata unknown;
    unknown.appVersion = "7.1.431.0";
    unknown.gsVersion = "Sunshine-2026.5";
    const auto detectedSunshine = HostCapabilityPolicy::detect(unknown);
    assert(detectedSunshine.kind == HostKind::Sunshine);
    assert(!detectedSunshine.virtualDisplay);

    HostMetadata explicitApollo;
    explicitApollo.gsVersion = "Apollo v0.3.6";
    const auto detectedApollo = HostCapabilityPolicy::detect(explicitApollo);
    assert(detectedApollo.kind == HostKind::Apollo);
    assert(detectedApollo.virtualDisplay);
    assert(detectedApollo.clipboardSync);

    HostMetadata extensions;
    extensions.appVersion = "Apollo";
    extensions.advertisedExtensions = "virtual-display,client-commands";
    const auto partial = HostCapabilityPolicy::detect(extensions);
    assert(partial.kind == HostKind::Apollo);
    assert(partial.virtualDisplay);
    assert(partial.serverCommands);
    assert(!partial.clipboardSync);
    assert(!partial.inputOnly);

    return 0;
}
