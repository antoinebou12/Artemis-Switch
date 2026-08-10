#include "../app/src/features/video/DisplayCoordinateMapper.hpp"

#include <cassert>
#include <cmath>

using namespace artemis::video;

int main() {
    DisplayTransform transform;
    auto plan = makeRendererPresentationPlan(
        720, 1280, 1280, 720, ScaleMode::Fit, transform, false);
    auto& mapper = DisplayCoordinateMapper::instance();
    mapper.update(1280, 720, plan, Rotation::Deg0);
    assert(!mapper.localToStream({0.1f, 0.5f}));
    auto center = mapper.localToStream({0.5f, 0.5f});
    assert(center && std::fabs(center->x - 0.5f) < 0.001f);
    assert(std::fabs(center->y - 0.5f) < 0.001f);

    transform.rotation = Rotation::Deg90;
    plan = makeRendererPresentationPlan(
        720, 1280, 1280, 720, ScaleMode::Fit, transform, false);
    mapper.update(1280, 720, plan, transform.rotation);
    auto topLeft = mapper.localToStream({0.0f, 0.0f});
    assert(topLeft && std::fabs(topLeft->x) < 0.001f);
    assert(std::fabs(topLeft->y - 1.0f) < 0.001f);

    transform.rotation = Rotation::Deg0;
    transform.zoom = 2.0f;
    transform.panX = 0.25f;
    plan = makeRendererPresentationPlan(
        1280, 720, 1280, 720, ScaleMode::Stretch, transform, false);
    mapper.update(1280, 720, plan, transform.rotation);
    center = mapper.localToStream({0.5f, 0.5f});
    assert(center && center->x > 0.5f);
    return 0;
}
