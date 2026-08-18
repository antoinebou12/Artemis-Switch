#include "HostCapabilities.hpp"

#include <cassert>

using namespace artemis::host;

int main() {
    const auto sunshine = HostCapabilityPolicy::standardSunshine();
    assert(sunshine.kind == HostKind::Sunshine);
    assert(sunshine.standardGameStream);
    assert(sunshine.gamepadInput);
    assert(!sunshine.extendedLaunchOptions);
    assert(!sunshine.preciseRefreshRate);
    assert(!sunshine.virtualDisplay);
    assert(!sunshine.serverCommands);
    assert(!sunshine.clipboardSync);
    assert(!sunshine.inputOnly);

    const auto apollo = HostCapabilityPolicy::apollo();
    assert(apollo.kind == HostKind::Apollo);
    assert(apollo.standardGameStream);
    assert(apollo.gamepadInput);
    assert(apollo.extendedLaunchOptions);
    assert(apollo.preciseRefreshRate);
    assert(apollo.virtualDisplay);
    assert(!apollo.serverCommands);
    assert(!apollo.clipboardSync);
    assert(!apollo.inputOnly);

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
    assert(!detectedApollo.clipboardSync);

    HostMetadata extensions;
    extensions.appVersion = "Apollo";
    extensions.advertisedExtensions = "virtual-display,client-commands";
    const auto partial = HostCapabilityPolicy::detect(extensions);
    assert(partial.kind == HostKind::Apollo);
    assert(partial.virtualDisplay);
    assert(partial.serverCommands);
    assert(!partial.clipboardSync);
    assert(!partial.inputOnly);

    const auto realApollo = HostCapabilityPolicy::fromServerInfo(
        HostKind::Apollo,
        true, false, true, 0x00130000, {"HDR", "Display 2"}, true);
    assert(realApollo.kind == HostKind::Apollo);
    assert(realApollo.virtualDisplay);
    assert(!realApollo.virtualDisplayDriverReady);
    assert(realApollo.clipboardSync);
    assert(realApollo.serverCommands);
    assert(realApollo.serverCommandList.size() == 2);

    const auto denied = HostCapabilityPolicy::fromServerInfo(
        HostKind::Apollo,
        true, true, true, 0, {"Unsafe"}, false);
    assert(!denied.clipboardSync);
    assert(!denied.serverCommands);

    const auto realSunshine = HostCapabilityPolicy::fromServerInfo(
        HostKind::Sunshine,
        false, false, false, 0, {}, false);
    assert(realSunshine.kind == HostKind::Sunshine);

    const auto extendedSunshine = HostCapabilityPolicy::fromServerInfo(
        HostKind::Sunshine,
        true, true, false, 0, {}, true);
    assert(extendedSunshine.kind == HostKind::Sunshine);
    assert(extendedSunshine.extendedLaunchOptions);
    assert(extendedSunshine.preciseRefreshRate);
    assert(extendedSunshine.virtualDisplay);
    assert(!extendedSunshine.clipboardSync);

    const auto vibeshine = HostCapabilityPolicy::fromServerInfo(
        HostKind::Vibeshine,
        true, true, false, 0, {}, true);
    assert(vibeshine.kind == HostKind::Vibeshine);
    assert(vibeshine.extendedLaunchOptions);
    assert(vibeshine.preciseRefreshRate);
    assert(vibeshine.virtualDisplay);
    assert(!vibeshine.clipboardSync);
    assert(!vibeshine.serverCommands);

    const auto vibeshineWithApolloFields = HostCapabilityPolicy::fromServerInfo(
        HostKind::Vibeshine,
        true, true, true, 0x00130000, {"Unsafe"}, true);
    assert(!vibeshineWithApolloFields.clipboardSync);
    assert(!vibeshineWithApolloFields.serverCommands);

    const auto punktfunk = HostCapabilityPolicy::punktfunk();
    assert(punktfunk.kind == HostKind::Punktfunk);
    assert(punktfunk.standardGameStream);
    assert(punktfunk.gamepadInput);
    assert(punktfunk.hostManagedVirtualDisplay);
    assert(!punktfunk.extendedLaunchOptions);

    // Version identity without field noise.
    HostMetadata vibepolloLike;
    vibepolloLike.gsVersion = "Vibepollo 1.0";
    assert(HostCapabilityPolicy::detect(vibepolloLike).kind == HostKind::Apollo);

    HostMetadata vibeshineMetadata;
    vibeshineMetadata.gsVersion = "Vibeshine 1.18";
    assert(HostCapabilityPolicy::detect(vibeshineMetadata).kind ==
           HostKind::Vibeshine);

    HostMetadata punktfunkMetadata;
    punktfunkMetadata.gsVersion = "punktfunk-host";
    assert(HostCapabilityPolicy::detect(punktfunkMetadata).kind ==
           HostKind::Punktfunk);

    HostMetadata apolloToken;
    apolloToken.gsVersion = "Apollo-fork";
    assert(HostCapabilityPolicy::detect(apolloToken).kind == HostKind::Apollo);

    return 0;
}
