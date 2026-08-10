#include "RendererPresentationPolicy.hpp"

#include <algorithm>
#include <cmath>

namespace artemis::video {

RendererPresentationPlan makeRendererPresentationPlan(
    int frameWidth, int frameHeight, int screenWidth, int screenHeight,
    ScaleMode scaleMode, const DisplayTransform& rawTransform,
    bool postProcessingEnabled) {
    RendererPresentationPlan result;
    if (frameWidth <= 0 || frameHeight <= 0 ||
        screenWidth <= 0 || screenHeight <= 0)
        return result;
    const auto transform = validateDisplayTransform(rawTransform);
    result.logicalSourceWidth = swapsDimensions(transform.rotation)
        ? frameHeight : frameWidth;
    result.logicalSourceHeight = swapsDimensions(transform.rotation)
        ? frameWidth : frameHeight;
    result.geometry = VideoScale::presentationGeometry(
        static_cast<float>(result.logicalSourceWidth),
        static_cast<float>(result.logicalSourceHeight),
        static_cast<float>(screenWidth), static_cast<float>(screenHeight),
        scaleMode, transform.zoom, transform.panX, transform.panY);
    result.postProcessWidth = postProcessingEnabled
        ? std::max(1, static_cast<int>(std::lround(
              result.geometry.destination.width)))
        : 0;
    result.postProcessHeight = postProcessingEnabled
        ? std::max(1, static_cast<int>(std::lround(
              result.geometry.destination.height)))
        : 0;
    result.clearLetterboxToBlack =
        result.geometry.destination.x > 0.0f ||
        result.geometry.destination.y > 0.0f ||
        result.geometry.destination.width < screenWidth ||
        result.geometry.destination.height < screenHeight;
    return result;
}

} // namespace artemis::video
