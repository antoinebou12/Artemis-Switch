#pragma once

#include <algorithm>
#include <cmath>

namespace artemis::input {

struct StickAxes {
    float x;
    float y;
};

// Radial deadzone that rescales the surviving range back over [0, 1].
//
// The previous behaviour zeroed anything below the threshold and passed
// everything else through untouched, so the first input past a 15% deadzone
// was already 15% deflection — the stick jumped instead of easing in.
//
// `magnitude` is passed in so callers can keep using their own (cheaper)
// square-root approximation. A deadzone of 0 returns the axes unchanged.
inline StickAxes applyRadialDeadzone(float x, float y, float magnitude,
                                     float deadzone) {
    if (deadzone <= 0.0f)
        return {x, y};

    // Leave a usable band above the threshold no matter what was configured.
    const float limit = std::min(deadzone, 0.95f);
    if (magnitude <= limit || magnitude <= 0.0f)
        return {0.0f, 0.0f};

    const float rescaled =
        std::min((magnitude - limit) / (1.0f - limit), 1.0f);
    const float scale = rescaled / magnitude;
    return {x * scale, y * scale};
}

inline StickAxes applyRadialDeadzone(float x, float y, float deadzone) {
    return applyRadialDeadzone(x, y, std::sqrt(x * x + y * y), deadzone);
}

} // namespace artemis::input
