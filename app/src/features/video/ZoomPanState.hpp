#pragma once

#include <algorithm>

namespace artemis::video {

struct ZoomPanState {
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
};

ZoomPanState normalizeZoomPan(ZoomPanState state);

inline float zoomToSlider(float zoom) {
    return std::clamp((zoom - 1.0f) / 3.0f, 0.0f, 1.0f);
}

inline float sliderToZoom(float progress) {
    return 1.0f + std::clamp(progress, 0.0f, 1.0f) * 3.0f;
}

inline float panToSlider(float pan) {
    return std::clamp((pan + 1.0f) / 2.0f, 0.0f, 1.0f);
}

inline float sliderToPan(float progress) {
    return std::clamp(progress, 0.0f, 1.0f) * 2.0f - 1.0f;
}

} // namespace artemis::video
