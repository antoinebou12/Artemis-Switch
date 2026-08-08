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

    const auto fitGeometry = VideoScale::presentationGeometry(
        1920, 1080, 1280, 800, ScaleMode::Fit);
    assert(near(fitGeometry.source.x, 0.0f));
    assert(near(fitGeometry.source.width, 1920.0f));
    assert(near(fitGeometry.destination.x, 0.0f));
    assert(near(fitGeometry.destination.y, 40.0f));
    assert(near(fitGeometry.destination.width, 1280.0f));
    assert(near(fitGeometry.destination.height, 720.0f));

    const auto fillGeometry = VideoScale::presentationGeometry(
        1920, 1080, 1280, 800, ScaleMode::Fill);
    assert(near(fillGeometry.destination.x, 0.0f));
    assert(near(fillGeometry.destination.y, 0.0f));
    assert(near(fillGeometry.destination.width, 1280.0f));
    assert(near(fillGeometry.destination.height, 800.0f));
    assert(fillGeometry.source.width < 1920.0f);
    assert(near(fillGeometry.source.height, 1080.0f));
    assert(fillGeometry.source.x > 0.0f);

    const auto stretchGeometry = VideoScale::presentationGeometry(
        1920, 1080, 1280, 800, ScaleMode::Stretch);
    assert(near(stretchGeometry.source.width, 1920.0f));
    assert(near(stretchGeometry.source.height, 1080.0f));
    assert(near(stretchGeometry.destination.width, 1280.0f));
    assert(near(stretchGeometry.destination.height, 800.0f));

    const auto zoomed = VideoScale::presentationGeometry(
        1920, 1080, 1280, 720, ScaleMode::Fit, 2.0f, 0.0f, 0.0f);
    assert(near(zoomed.source.width, 960.0f));
    assert(near(zoomed.source.height, 540.0f));
    assert(near(zoomed.source.x, 480.0f));
    assert(near(zoomed.source.y, 270.0f));

    const auto panned = VideoScale::presentationGeometry(
        1920, 1080, 1280, 720, ScaleMode::Fit, 2.0f, 0.5f, -0.5f);
    assert(near(panned.source.x, 960.0f));
    assert(near(panned.source.y, 0.0f));
    assert(near(panned.source.width, 960.0f));
    assert(near(panned.source.height, 540.0f));

    const auto invalid = VideoScale::presentationGeometry(
        0, 1080, 1280, 720, ScaleMode::Fit);
    assert(near(invalid.source.width, 0.0f));
    assert(near(invalid.destination.width, 0.0f));

    return 0;
}
