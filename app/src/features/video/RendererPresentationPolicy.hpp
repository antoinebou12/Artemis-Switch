#pragma once

#include "DisplayTransform.hpp"
#include "../../video/VideoScale.hpp"

namespace artemis::video {

struct RendererPresentationPlan {
    int logicalSourceWidth = 0;
    int logicalSourceHeight = 0;
    PresentationGeometry geometry;
    int postProcessWidth = 0;
    int postProcessHeight = 0;
    bool clearLetterboxToBlack = false;
};

RendererPresentationPlan makeRendererPresentationPlan(
    int frameWidth, int frameHeight, int screenWidth, int screenHeight,
    ScaleMode scaleMode, const DisplayTransform& transform,
    bool postProcessingEnabled);

} // namespace artemis::video
