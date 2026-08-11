#include "FrameRateOptions.hpp"

#include <cmath>

namespace artemis::stream {
namespace {

bool isNtscX100(int x100) {
    return x100 == 2997 || x100 == 5994 || x100 == 11988;
}

} // namespace

std::vector<FrameRatePreset> availableFrameRatePresets() {
    return {
        {30, 0, "30 FPS"},
        {40, 0, "40 FPS"},
        {60, 5994, "59.94 FPS (NTSC)"},
        {60, 0, "60 FPS"},
        {90, 0, "90 FPS"},
        {120, 11988, "119.88 FPS (NTSC)"},
        {120, 0, "120 FPS"},
    };
}

int resolveClientRefreshRateX100(int fps, int clientRefreshRateX100) {
    if (clientRefreshRateX100 > 0)
        return clientRefreshRateX100;
    if (fps <= 0)
        return 6000;
    return fps * 100;
}

int launchModeFpsValue(int fps, int clientRefreshRateX100, bool apolloHost) {
    const int x100 = resolveClientRefreshRateX100(fps, clientRefreshRateX100);
    if (apolloHost && isNtscX100(x100)) {
        // Artemis Classic encodes fractional rates as millihertz in mode=.
        return x100 * 10;
    }
    return fps > 0 ? fps : 60;
}

int frameRatePresetIndex(int fps, int clientRefreshRateX100) {
    const auto presets = availableFrameRatePresets();
    const int x100 = clientRefreshRateX100 > 0 ? clientRefreshRateX100 : 0;
    for (size_t i = 0; i < presets.size(); ++i) {
        if (presets[i].fps == fps && presets[i].clientRefreshRateX100 == x100)
            return static_cast<int>(i);
    }
    // Prefer matching fps with integer (x100==0) entry.
    for (size_t i = 0; i < presets.size(); ++i) {
        if (presets[i].fps == fps && presets[i].clientRefreshRateX100 == 0)
            return static_cast<int>(i);
    }
    int best = 3; // 60 FPS
    int bestDistance = std::abs(presets[best].fps - fps);
    for (size_t i = 0; i < presets.size(); ++i) {
        const int distance = std::abs(presets[i].fps - fps);
        if (distance < bestDistance) {
            best = static_cast<int>(i);
            bestDistance = distance;
        }
    }
    return best;
}

std::string frameRatePresetLabel(const FrameRatePreset& preset) {
    return preset.labelKey;
}

} // namespace artemis::stream
