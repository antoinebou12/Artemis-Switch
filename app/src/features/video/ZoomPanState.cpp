#include "ZoomPanState.hpp"

#include <algorithm>

namespace artemis::video {

ZoomPanState normalizeZoomPan(ZoomPanState state) {
    state.zoom = std::clamp(state.zoom, 1.0f, 4.0f);

    // Pan is stored in normalized viewport coordinates. At 1x there is no
    // meaningful pan. As zoom grows, the legal pan range grows toward +/-1.
    const float maxPan = 1.0f - (1.0f / state.zoom);
    state.panX = std::clamp(state.panX, -maxPan, maxPan);
    state.panY = std::clamp(state.panY, -maxPan, maxPan);
    return state;
}

} // namespace artemis::video
