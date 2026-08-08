#include "VideoScale.hpp"

#include <cassert>
#include <cmath>

using namespace artemis::video;

static bool near(float a, float b) {
    return std::fabs(a - b) < 0.001f;
}

int main() {
    const auto fit = VideoScale::destinationRect(1920, 1080, 1280, 800, ScaleMode::Fit);
    assert(near(fit.width, 1280.0f));
    assert(near(fit.height, 720.0f));
    assert(near(fit.x, 0.0f));
    assert(near(fit.y, 40.0f));

    const auto fill = VideoScale::destinationRect(1920, 1080, 1280, 800, ScaleMode::Fill);
    assert(near(fill.height, 800.0f));
    assert(fill.width > 1280.0f);
    assert(fill.x < 0.0f);

    const auto stretch = VideoScale::destinationRect(1920, 1080, 1280, 800, ScaleMode::Stretch);
    assert(near(stretch.width, 1280.0f));
    assert(near(stretch.height, 800.0f));

    const auto invalid = VideoScale::destinationRect(0, 1080, 1280, 720, ScaleMode::Fit);
    assert(near(invalid.width, 0.0f));
    assert(near(invalid.height, 0.0f));

    return 0;
}
