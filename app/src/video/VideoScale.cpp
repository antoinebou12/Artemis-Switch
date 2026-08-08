#include "VideoScale.hpp"

#include <algorithm>

namespace artemis::video {

RectF VideoScale::destinationRect(float sourceWidth, float sourceHeight,
                                  float viewportWidth, float viewportHeight,
                                  ScaleMode mode) {
    if (sourceWidth <= 0.0f || sourceHeight <= 0.0f ||
        viewportWidth <= 0.0f || viewportHeight <= 0.0f)
        return {};

    if (mode == ScaleMode::Stretch)
        return {0.0f, 0.0f, viewportWidth, viewportHeight};

    const float scaleX = viewportWidth / sourceWidth;
    const float scaleY = viewportHeight / sourceHeight;
    const float scale = mode == ScaleMode::Fit ? std::min(scaleX, scaleY)
                                                : std::max(scaleX, scaleY);
    const float width = sourceWidth * scale;
    const float height = sourceHeight * scale;
    return {(viewportWidth - width) * 0.5f,
            (viewportHeight - height) * 0.5f,
            width,
            height};
}

} // namespace artemis::video
