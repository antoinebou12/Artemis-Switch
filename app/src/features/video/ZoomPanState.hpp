#pragma once

namespace artemis::video {

struct ZoomPanState {
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
};

ZoomPanState normalizeZoomPan(ZoomPanState state);

} // namespace artemis::video
