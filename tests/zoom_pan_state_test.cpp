#include "../app/src/features/video/ZoomPanState.hpp"

#include <cassert>
#include <cmath>

using artemis::video::ZoomPanState;
using artemis::video::normalizeZoomPan;

int main() {
    auto state = normalizeZoomPan({0.5f, 1.0f, -1.0f});
    assert(state.zoom == 1.0f);
    assert(state.panX == 0.0f);
    assert(state.panY == 0.0f);

    state = normalizeZoomPan({2.0f, 0.9f, -0.9f});
    assert(std::fabs(state.panX - 0.5f) < 0.0001f);
    assert(std::fabs(state.panY + 0.5f) < 0.0001f);

    state = normalizeZoomPan({8.0f, 2.0f, -2.0f});
    assert(state.zoom == 4.0f);
    assert(std::fabs(state.panX - 0.75f) < 0.0001f);
    assert(std::fabs(state.panY + 0.75f) < 0.0001f);
    return 0;
}
