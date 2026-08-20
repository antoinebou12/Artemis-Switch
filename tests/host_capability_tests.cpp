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
    assert(HostCapabilityPolicy::detect(vibepolloLike).kind ==
           HostKind::Vibepollo);

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

    // --- Vibepollo -------------------------------------------------------
    const auto vibepollo = HostCapabilityPolicy::vibepollo();
    assert(vibepollo.kind == HostKind::Vibepollo);
    assert(vibepollo.standardGameStream);
    assert(vibepollo.gamepadInput);
    assert(vibepollo.extendedLaunchOptions);
    assert(vibepollo.preciseRefreshRate);
    assert(vibepollo.virtualDisplay);

    assert(isApolloFamily(HostKind::Apollo));
    assert(isApolloFamily(HostKind::Vibepollo));
    assert(!isApolloFamily(HostKind::Sunshine));
    assert(!isApolloFamily(HostKind::Vibeshine));
    assert(!isApolloFamily(HostKind::Punktfunk));

    // Detected from every version field, not only gsVersion.
    for (int field = 0; field < 3; ++field) {
        HostMetadata m;
        if (field == 0) m.appVersion = "Vibepollo 1.2.3";
        if (field == 1) m.gfeVersion = "vibepollo/1.2.3";
        if (field == 2) m.gsVersion = "VIBEPOLLO";
        assert(HostCapabilityPolicy::detect(m).kind == HostKind::Vibepollo);
    }

    // Apollo-gated extensions must survive the family split.
    const auto vibepolloFields = HostCapabilityPolicy::fromServerInfo(
        HostKind::Vibepollo, true, true, true,
        0x00010000 | 0x00100000, {"restart", "reset-display"}, true);
    assert(vibepolloFields.kind == HostKind::Vibepollo);
    assert(vibepolloFields.serverCommands);
    assert(vibepolloFields.clipboardSync);
    assert(vibepolloFields.inputOnly);
    assert(vibepolloFields.virtualDisplay);
    assert(vibepolloFields.virtualDisplayDriverReady);

    // An extension list narrows Vibepollo the same way it narrows Apollo.
    HostMetadata vibepolloExtensions;
    vibepolloExtensions.appVersion = "Vibepollo";
    vibepolloExtensions.advertisedExtensions = "virtual-display,clipboard";
    const auto vibepolloPartial =
        HostCapabilityPolicy::detect(vibepolloExtensions);
    assert(vibepolloPartial.kind == HostKind::Vibepollo);
    assert(vibepolloPartial.virtualDisplay);
    assert(vibepolloPartial.clipboardSync);
    assert(!vibepolloPartial.serverCommands);

    // --- Newer Sunshine-class hosts --------------------------------------
    struct { const char* version; HostKind kind; } plainForks[] = {
        {"Polaris 0.4", HostKind::Polaris},
        {"Solar-Flare 1.0", HostKind::SolarFlare},
        {"SolarFlare 1.0", HostKind::SolarFlare},
        {"foundation-sunshine 2026.1", HostKind::FoundationSunshine},
    };
    for (const auto& fork : plainForks) {
        HostMetadata m;
        m.gsVersion = fork.version;
        const auto detected = HostCapabilityPolicy::detect(m);
        assert(detected.kind == fork.kind);
        assert(detected.standardGameStream);
        assert(detected.gamepadInput);
        assert(!detected.extendedLaunchOptions);
        assert(!detected.virtualDisplay);
        assert(!detected.clipboardSync);
        assert(!detected.serverCommands);
    }

    // They are not Apollo, so Apollo-only fields stay off even when present.
    const auto polarisFields = HostCapabilityPolicy::fromServerInfo(
        HostKind::Polaris, false, false, true, 0x00110000,
        {"restart"}, true);
    assert(polarisFields.kind == HostKind::Polaris);
    assert(!polarisFields.serverCommands);
    assert(!polarisFields.clipboardSync);
    assert(!polarisFields.inputOnly);

    // Web console ports and product names.
    assert(HostCapabilityPolicy::identityFor(HostKind::Vibepollo).product ==
           "Vibepollo");
    assert(HostCapabilityPolicy::identityFor(HostKind::Vibepollo)
               .webConsolePort == 47990);
    assert(HostCapabilityPolicy::identityFor(HostKind::Polaris).product ==
           "Polaris");
    assert(HostCapabilityPolicy::identityFor(HostKind::SolarFlare).product ==
           "Solar Flare");
    assert(HostCapabilityPolicy::identityFor(HostKind::FoundationSunshine)
               .product == "Foundation Sunshine");
    assert(HostCapabilityPolicy::identityFor(HostKind::Punktfunk)
               .webConsolePort == 47992);

    return 0;
}
