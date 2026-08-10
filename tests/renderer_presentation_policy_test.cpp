#include "../app/src/features/video/RendererPresentationPolicy.hpp"

#include <array>
#include <cassert>

using namespace artemis::video;

int main() {
    constexpr std::array modes = {
        ScaleMode::Fit, ScaleMode::Fill, ScaleMode::Stretch};
    constexpr std::array rotations = {
        Rotation::Deg0, Rotation::Deg90,
        Rotation::Deg180, Rotation::Deg270};
    for (const auto mode : modes) {
        for (const auto rotation : rotations) {
            for (const float zoom : {1.0f, 2.0f}) {
                for (const bool post : {false, true}) {
                    DisplayTransform transform;
                    transform.rotation = rotation;
                    transform.zoom = zoom;
                    transform.panX = zoom > 1.0f ? 0.25f : 0.0f;
                    const auto plan = makeRendererPresentationPlan(
                        720, 1280, 1280, 720, mode, transform, post);
                    assert(plan.logicalSourceWidth ==
                           (swapsDimensions(rotation) ? 1280 : 720));
                    assert(plan.geometry.source.width > 0);
                    assert(plan.geometry.destination.width > 0);
                    assert((plan.postProcessWidth > 0) == post);
                    assert((plan.postProcessHeight > 0) == post);
                    if (mode == ScaleMode::Fit &&
                        rotation != Rotation::Deg90 &&
                        rotation != Rotation::Deg270)
                        assert(plan.clearLetterboxToBlack);
                }
            }
        }
    }
    return 0;
}
