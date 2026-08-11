#include "ApolloHostOptions.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>

namespace artemis::apollo {
namespace {
void fitDecodeBudget(int& width, int& height) {
    width = std::clamp(width, 360, 1920);
    height = std::clamp(height, 360, 1920);
    constexpr double budget = 1920.0 * 1080.0;
    const double pixels = static_cast<double>(width) * height;
    if (pixels > budget) {
        const double scale = std::sqrt(budget / pixels);
        width = std::max(360, static_cast<int>(width * scale) & ~1);
        height = std::max(360, static_cast<int>(height * scale) & ~1);
    }
    width &= ~1;
    height &= ~1;
}
}

ApolloHostOptions validateApolloHostOptions(ApolloHostOptions options) {
    fitDecodeBudget(options.customWidth, options.customHeight);
    options.refreshRate = std::clamp(options.refreshRate, 30, 120);
    static constexpr int kAllowedScale[] = {50, 75, 100, 125, 150};
    bool scaleOk = false;
    for (int allowed : kAllowedScale) {
        if (options.scaleFactor == allowed) {
            scaleOk = true;
            break;
        }
    }
    if (!scaleOk)
        options.scaleFactor = 100;
    return options;
}

ResolvedVirtualDisplay resolveVirtualDisplay(const ApolloHostOptions& raw,
                                             int profileWidth,
                                             int profileHeight) {
    const auto options = validateApolloHostOptions(raw);
    ResolvedVirtualDisplay result;
    result.enabled = options.target != VirtualDisplayTarget::Off;
    result.refreshRate = options.refreshRate;
    switch (options.target) {
    case VirtualDisplayTarget::Off: return {};
    case VirtualDisplayTarget::CurrentProfile:
        result.width = profileWidth; result.height = profileHeight; break;
    case VirtualDisplayTarget::Handheld:
        result.width = 1280; result.height = 720; break;
    case VirtualDisplayTarget::Docked:
        result.width = 1920; result.height = 1080; break;
    case VirtualDisplayTarget::PortraitHandheld:
        result.width = 720; result.height = 1280; break;
    case VirtualDisplayTarget::PortraitDocked:
        result.width = 1080; result.height = 1920; break;
    case VirtualDisplayTarget::Custom:
        result.width = options.customWidth; result.height = options.customHeight; break;
    }
    fitDecodeBudget(result.width, result.height);
    return result;
}

const char* virtualDisplayTargetName(VirtualDisplayTarget target) {
    switch (target) {
    case VirtualDisplayTarget::Off: return "off";
    case VirtualDisplayTarget::CurrentProfile: return "current-profile";
    case VirtualDisplayTarget::Handheld: return "handheld";
    case VirtualDisplayTarget::Docked: return "docked";
    case VirtualDisplayTarget::PortraitHandheld: return "portrait-handheld";
    case VirtualDisplayTarget::PortraitDocked: return "portrait-docked";
    case VirtualDisplayTarget::Custom: return "custom";
    }
    return "off";
}

bool virtualDisplayTargetFromName(std::string_view name,
                                  VirtualDisplayTarget& target) {
    for (const auto candidate : {VirtualDisplayTarget::Off,
                                 VirtualDisplayTarget::CurrentProfile,
                                 VirtualDisplayTarget::Handheld,
                                 VirtualDisplayTarget::Docked,
                                 VirtualDisplayTarget::PortraitHandheld,
                                 VirtualDisplayTarget::PortraitDocked,
                                 VirtualDisplayTarget::Custom}) {
        if (name == virtualDisplayTargetName(candidate)) {
            target = candidate;
            return true;
        }
    }
    return false;
}

bool parseVirtualDisplaySpec(std::string_view text, ApolloHostOptions& options,
                             std::string* error) {
    const auto fail = [error](const char* message) {
        if (error) *error = message;
        return false;
    };
    const size_t x = text.find_first_of("xX");
    const size_t at = text.find('@', x == std::string_view::npos ? 0 : x + 1);
    if (x == std::string_view::npos || at == std::string_view::npos ||
        x == 0 || at <= x + 1 || at + 1 >= text.size())
        return fail("Expected WIDTHxHEIGHT@HZ");

    int width = 0;
    int height = 0;
    int refresh = 0;
    const auto parse = [](std::string_view value, int& output) {
        const char* first = value.data();
        const char* last = first + value.size();
        const auto result = std::from_chars(first, last, output);
        return result.ec == std::errc{} && result.ptr == last;
    };
    if (!parse(text.substr(0, x), width) ||
        !parse(text.substr(x + 1, at - x - 1), height) ||
        !parse(text.substr(at + 1), refresh))
        return fail("Resolution and refresh rate must be numbers");
    if (width < 360 || width > 1920 || height < 360 || height > 1920 ||
        (width & 1) || (height & 1) ||
        static_cast<int64_t>(width) * height > 1920LL * 1080LL)
        return fail("Resolution exceeds the Switch decode budget");
    if (refresh < 30 || refresh > 120)
        return fail("Refresh rate must be between 30 and 120 Hz");

    options.target = VirtualDisplayTarget::Custom;
    options.customWidth = width;
    options.customHeight = height;
    options.refreshRate = refresh;
    if (error) error->clear();
    return true;
}

} // namespace artemis::apollo
