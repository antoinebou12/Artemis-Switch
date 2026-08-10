#include "StatsOverlayLayout.hpp"

namespace artemis::ui {

StatsOverlayOrigin stats_overlay_origin(StatsCorner corner, float width,
                                        float height, float margin,
                                        float topY) {
    StatsOverlayOrigin origin;
    origin.alignRight = corner == StatsCorner::TopRight ||
                        corner == StatsCorner::BottomRight;
    origin.alignBottom = corner == StatsCorner::BottomLeft ||
                         corner == StatsCorner::BottomRight;
    origin.x = origin.alignRight ? width - margin : margin;
    origin.y = origin.alignBottom ? height - margin : topY;
    return origin;
}

} // namespace artemis::ui
