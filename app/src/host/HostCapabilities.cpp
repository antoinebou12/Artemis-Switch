#include "HostCapabilities.hpp"

#include <algorithm>
#include <cctype>

namespace artemis::host {
namespace {
std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool contains(const std::string& haystack, const char* needle) {
    return lowercase(haystack).find(needle) != std::string::npos;
}

bool extension(const std::string& extensions, const char* token) {
    return contains(extensions, token);
}
}

bool isApolloFamily(HostKind kind) {
    return kind == HostKind::Apollo || kind == HostKind::Vibepollo;
}

HostCapabilities HostCapabilityPolicy::standardSunshine() {
    HostCapabilities c;
    c.kind = HostKind::Sunshine;
    c.standardGameStream = true;
    c.gamepadInput = true;
    c.detectionReason = "standard GameStream/Sunshine fallback";
    return c;
}

HostCapabilities HostCapabilityPolicy::apollo() {
    HostCapabilities c;
    c.kind = HostKind::Apollo;
    c.standardGameStream = true;
    c.gamepadInput = true;
    c.extendedLaunchOptions = true;
    c.preciseRefreshRate = true;
    c.virtualDisplay = true;
    c.detectionReason = "explicit Apollo identity";
    return c;
}

HostCapabilities HostCapabilityPolicy::vibeshine() {
    HostCapabilities c;
    c.kind = HostKind::Vibeshine;
    c.standardGameStream = true;
    c.gamepadInput = true;
    c.extendedLaunchOptions = true;
    c.preciseRefreshRate = true;
    c.detectionReason = "explicit Vibeshine identity";
    return c;
}

HostCapabilities HostCapabilityPolicy::punktfunk() {
    HostCapabilities c;
    c.kind = HostKind::Punktfunk;
    c.standardGameStream = true;
    c.gamepadInput = true;
    c.hostManagedVirtualDisplay = true;
    c.detectionReason = "Punktfunk health probe";
    return c;
}

HostCapabilities HostCapabilityPolicy::vibepollo() {
    HostCapabilities c = apollo();
    c.kind = HostKind::Vibepollo;
    c.detectionReason = "explicit Vibepollo identity";
    return c;
}

HostCapabilities HostCapabilityPolicy::polaris() {
    HostCapabilities c = standardSunshine();
    c.kind = HostKind::Polaris;
    c.detectionReason = "explicit Polaris identity";
    return c;
}

HostCapabilities HostCapabilityPolicy::solarFlare() {
    HostCapabilities c = standardSunshine();
    c.kind = HostKind::SolarFlare;
    c.detectionReason = "explicit Solar Flare identity";
    return c;
}

HostCapabilities HostCapabilityPolicy::foundationSunshine() {
    HostCapabilities c = standardSunshine();
    c.kind = HostKind::FoundationSunshine;
    c.detectionReason = "explicit Foundation Sunshine identity";
    return c;
}

HostIdentity HostCapabilityPolicy::identityFor(HostKind kind) {
    HostIdentity identity;
    identity.kind = kind;
    switch (kind) {
        case HostKind::Sunshine:
            identity.product = "Sunshine-compatible";
            break;
        case HostKind::Apollo:
            identity.product = "Apollo";
            break;
        case HostKind::Vibeshine:
            identity.product = "Vibeshine";
            break;
        case HostKind::Punktfunk:
            identity.product = "Punktfunk";
            identity.webConsolePort = 47992;
            break;
        case HostKind::Vibepollo:
            identity.product = "Vibepollo";
            break;
        case HostKind::Polaris:
            identity.product = "Polaris";
            break;
        case HostKind::SolarFlare:
            identity.product = "Solar Flare";
            break;
        case HostKind::FoundationSunshine:
            identity.product = "Foundation Sunshine";
            break;
        case HostKind::Unknown:
            identity.product = "GameStream host";
            break;
    }
    return identity;
}

HostCapabilities HostCapabilityPolicy::detect(const HostMetadata& metadata) {
    const bool explicitVibeshine =
        contains(metadata.appVersion, "vibeshine") ||
        contains(metadata.gfeVersion, "vibeshine") ||
        contains(metadata.gsVersion, "vibeshine") ||
        extension(metadata.advertisedExtensions, "vibeshine");
    if (explicitVibeshine)
        return vibeshine();

    const bool explicitPunktfunk =
        contains(metadata.appVersion, "punktfunk") ||
        contains(metadata.gfeVersion, "punktfunk") ||
        contains(metadata.gsVersion, "punktfunk") ||
        extension(metadata.advertisedExtensions, "punktfunk");
    if (explicitPunktfunk)
        return punktfunk();

    // Vibepollo is an Apollo fork, so it must be matched before the generic
    // Apollo check -- "vibepollo" also contains "pollo" but not "apollo".
    const bool explicitVibepollo =
        contains(metadata.appVersion, "vibepollo") ||
        contains(metadata.gfeVersion, "vibepollo") ||
        contains(metadata.gsVersion, "vibepollo") ||
        extension(metadata.advertisedExtensions, "vibepollo");

    const bool explicitApollo = explicitVibepollo ||
        contains(metadata.appVersion, "apollo") ||
        contains(metadata.gfeVersion, "apollo") ||
        contains(metadata.gsVersion, "apollo") ||
        extension(metadata.advertisedExtensions, "apollo");

    if (!explicitApollo) {
        const bool explicitPolaris =
            contains(metadata.appVersion, "polaris") ||
            contains(metadata.gfeVersion, "polaris") ||
            contains(metadata.gsVersion, "polaris") ||
            extension(metadata.advertisedExtensions, "polaris");
        if (explicitPolaris)
            return polaris();

        const bool explicitSolarFlare =
            contains(metadata.appVersion, "solar-flare") ||
            contains(metadata.appVersion, "solarflare") ||
            contains(metadata.gfeVersion, "solar-flare") ||
            contains(metadata.gfeVersion, "solarflare") ||
            contains(metadata.gsVersion, "solar-flare") ||
            contains(metadata.gsVersion, "solarflare") ||
            extension(metadata.advertisedExtensions, "solar-flare") ||
            extension(metadata.advertisedExtensions, "solarflare");
        if (explicitSolarFlare)
            return solarFlare();

        const bool explicitFoundation =
            contains(metadata.appVersion, "foundation") ||
            contains(metadata.gfeVersion, "foundation") ||
            contains(metadata.gsVersion, "foundation") ||
            extension(metadata.advertisedExtensions, "foundation");
        if (explicitFoundation)
            return foundationSunshine();

        return standardSunshine();
    }

    HostCapabilities result = explicitVibepollo ? vibepollo() : apollo();

    if (!metadata.advertisedExtensions.empty()) {
        // When an extension list is present, do not assume every Apollo feature.
        result.virtualDisplay = extension(metadata.advertisedExtensions, "virtual-display") ||
                                extension(metadata.advertisedExtensions, "virtual_display");
        result.serverCommands = extension(metadata.advertisedExtensions, "client-commands") ||
                                extension(metadata.advertisedExtensions, "server-commands");
        result.clipboardSync = extension(metadata.advertisedExtensions, "clipboard");
        result.inputOnly = extension(metadata.advertisedExtensions, "input-only") ||
                           extension(metadata.advertisedExtensions, "input_only");
        result.detectionReason = explicitVibepollo
            ? "Vibepollo identity with explicit extension list"
            : "Apollo identity with explicit extension list";
    }

    return result;
}

HostCapabilities HostCapabilityPolicy::fromServerInfo(
    HostKind identity,
    bool virtualDisplayCapable, bool virtualDisplayDriverReady,
    bool permissionAdvertised, uint32_t permissions,
    std::vector<std::string> serverCommands,
    bool currentAppHasUuid) {
    constexpr uint32_t clipboardSet = 0x00010000;
    constexpr uint32_t clipboardRead = 0x00020000;
    constexpr uint32_t serverCommand = 0x00100000;
    HostCapabilities result;
    switch (identity) {
        case HostKind::Apollo: result = apollo(); break;
        case HostKind::Vibepollo: result = vibepollo(); break;
        case HostKind::Vibeshine: result = vibeshine(); break;
        case HostKind::Punktfunk: result = punktfunk(); break;
        case HostKind::Polaris: result = polaris(); break;
        case HostKind::SolarFlare: result = solarFlare(); break;
        case HostKind::FoundationSunshine: result = foundationSunshine(); break;
        case HostKind::Sunshine:
        case HostKind::Unknown: result = standardSunshine(); break;
    }

    // Permission and command fields are Apollo-specific evidence. Virtual
    // display and app UUID fields are also advertised by Vibeshine, so they
    // must not grant Apollo clipboard access on their own.
    if (result.kind == HostKind::Sunshine &&
        (permissionAdvertised || !serverCommands.empty())) {
        result = apollo();
    }

    if (virtualDisplayCapable) {
        result.virtualDisplay = true;
        result.extendedLaunchOptions = true;
        result.preciseRefreshRate = true;
    }
    result.virtualDisplayDriverReady = virtualDisplayDriverReady;
    result.permissionAdvertised = permissionAdvertised;
    result.permissions = permissions;
    result.serverCommandList = std::move(serverCommands);
    result.serverCommands = isApolloFamily(result.kind) &&
        !result.serverCommandList.empty() &&
        (!permissionAdvertised || (permissions & serverCommand) != 0);
    result.clipboardSync = isApolloFamily(result.kind) &&
        permissionAdvertised &&
        (permissions & (clipboardSet | clipboardRead)) != 0;
    result.inputOnly = isApolloFamily(result.kind) && currentAppHasUuid;
    if (result.kind == HostKind::Vibepollo)
        result.detectionReason = "Vibepollo identity or server-info fields";
    else if (result.kind == HostKind::Apollo)
        result.detectionReason = "Apollo identity or server-info fields";
    else if (result.kind == HostKind::Vibeshine)
        result.detectionReason = "Vibeshine identity with server-info fields";
    return result;
}

} // namespace artemis::host
