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

HostCapabilities HostCapabilityPolicy::standardSunshine() {
    HostCapabilities c;
    c.kind = HostKind::Sunshine;
    c.standardGameStream = true;
    c.detectionReason = "standard GameStream/Sunshine fallback";
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
    c.detectionReason = "explicit Apollo identity";
    return c;
}

HostCapabilities HostCapabilityPolicy::detect(const HostMetadata& metadata) {
    const bool explicitApollo =
        contains(metadata.appVersion, "apollo") ||
        contains(metadata.gfeVersion, "apollo") ||
        contains(metadata.gsVersion, "apollo") ||
        extension(metadata.advertisedExtensions, "apollo");

    HostCapabilities result = explicitApollo ? apollo() : standardSunshine();
    if (!explicitApollo)
        return result;

    if (!metadata.advertisedExtensions.empty()) {
        // When an extension list is present, do not assume every Apollo feature.
        result.virtualDisplay = extension(metadata.advertisedExtensions, "virtual-display") ||
                                extension(metadata.advertisedExtensions, "virtual_display");
        result.serverCommands = extension(metadata.advertisedExtensions, "client-commands") ||
                                extension(metadata.advertisedExtensions, "server-commands");
        result.clipboardSync = extension(metadata.advertisedExtensions, "clipboard");
        result.inputOnly = extension(metadata.advertisedExtensions, "input-only") ||
                           extension(metadata.advertisedExtensions, "input_only");
        result.detectionReason = "Apollo identity with explicit extension list";
    }

    return result;
}

} // namespace artemis::host
