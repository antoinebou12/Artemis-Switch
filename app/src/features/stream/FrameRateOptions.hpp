#pragma once

#include <string>
#include <vector>

namespace artemis::stream {

// Selectable stream frame-rate presets. Integer rates use fps as-is; NTSC
// presets keep STREAM_CONFIGURATION::fps at the nearest integer while
// advertising the fractional rate via clientRefreshRateX100 / Apollo mode.
struct FrameRatePreset {
    int fps = 60;
    // 0 means "use fps * 100" (integer rate).
    int clientRefreshRateX100 = 0;
    const char* labelKey = "artemis/settings/fps_60";
};

std::vector<FrameRatePreset> availableFrameRatePresets();

// Resolve clientRefreshRateX100 for STREAM_CONFIGURATION (never 0).
int resolveClientRefreshRateX100(int fps, int clientRefreshRateX100);

// Apollo / Artemis Classic millihertz third component for mode=WxHxF.
// Integer rates stay as fps; fractional NTSC rates become e.g. 59940.
int launchModeFpsValue(int fps, int clientRefreshRateX100, bool apolloHost);

// Map a stored (fps, x100) pair onto a preset index for UI selectors.
int frameRatePresetIndex(int fps, int clientRefreshRateX100);

std::string frameRatePresetLabel(const FrameRatePreset& preset);

} // namespace artemis::stream
