#pragma once

namespace artemis::ui {

enum class StatsCorner : int {
    TopLeft = 0,
    TopRight = 1,
    BottomLeft = 2,
    BottomRight = 3,
};

struct StatsOverlayOrigin {
    float x = 20.f;
    float y = 30.f;
    bool alignRight = false;
    bool alignBottom = false;
};

StatsOverlayOrigin stats_overlay_origin(StatsCorner corner, float width,
                                        float height, float margin = 20.f,
                                        float topY = 30.f);

} // namespace artemis::ui
