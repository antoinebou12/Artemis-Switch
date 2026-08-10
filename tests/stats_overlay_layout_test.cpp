#include "StatsOverlayLayout.hpp"

#include <cassert>
#include <iostream>

int main() {
    using artemis::ui::StatsCorner;
    using artemis::ui::stats_overlay_origin;

    auto topLeft = stats_overlay_origin(StatsCorner::TopLeft, 1280, 720);
    assert(topLeft.x == 20.f);
    assert(topLeft.y == 30.f);
    assert(!topLeft.alignRight);
    assert(!topLeft.alignBottom);

    auto topRight = stats_overlay_origin(StatsCorner::TopRight, 1280, 720);
    assert(topRight.x == 1260.f);
    assert(topRight.alignRight);
    assert(!topRight.alignBottom);

    auto bottomLeft = stats_overlay_origin(StatsCorner::BottomLeft, 1280, 720);
    assert(bottomLeft.y == 700.f);
    assert(!bottomLeft.alignRight);
    assert(bottomLeft.alignBottom);

    auto bottomRight =
        stats_overlay_origin(StatsCorner::BottomRight, 1280, 720);
    assert(bottomRight.x == 1260.f);
    assert(bottomRight.y == 700.f);
    assert(bottomRight.alignRight);
    assert(bottomRight.alignBottom);

    std::cout << "stats_overlay_layout_test ok\n";
    return 0;
}
