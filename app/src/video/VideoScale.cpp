#include "VideoScale.hpp"

#include <algorithm>

namespace artemis::video {

ScaleMode nextScaleMode(ScaleMode mode) {
    switch (mode) {
    case ScaleMode::Fit:
        return ScaleMode::Fill;
    case ScaleMode::Fill:
        return ScaleMode::Stretch;
    case ScaleMode::Stretch:
    default:
        return ScaleMode::Fit;
    }
}

bool usesFilteredFullScreenPath(ScaleMode mode) {
    return mode == ScaleMode::Fill || mode == ScaleMode::Stretch;
}

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

PresentationGeometry VideoScale::presentationGeometry(
    float sourceWidth, float sourceHeight,
    float viewportWidth, float viewportHeight,
    ScaleMode mode, float zoom, float panX, float panY) {
    PresentationGeometry result;
    if (sourceWidth <= 0.0f || sourceHeight <= 0.0f ||
        viewportWidth <= 0.0f || viewportHeight <= 0.0f)
        return result;

    result.source = {0.0f, 0.0f, sourceWidth, sourceHeight};

    if (mode == ScaleMode::Fit) {
        result.destination = destinationRect(sourceWidth, sourceHeight,
                                             viewportWidth, viewportHeight,
                                             ScaleMode::Fit);
    } else {
        // Fill is implemented as a source crop into a full-screen destination.
        // Stretch also uses the full destination but keeps the complete source.
        result.destination = {0.0f, 0.0f, viewportWidth, viewportHeight};

        if (mode == ScaleMode::Fill) {
            const float sourceAspect = sourceWidth / sourceHeight;
            const float viewportAspect = viewportWidth / viewportHeight;
            if (sourceAspect > viewportAspect) {
                const float visibleWidth = sourceHeight * viewportAspect;
                result.source.x = (sourceWidth - visibleWidth) * 0.5f;
                result.source.width = visibleWidth;
            } else if (sourceAspect < viewportAspect) {
                const float visibleHeight = sourceWidth / viewportAspect;
                result.source.y = (sourceHeight - visibleHeight) * 0.5f;
                result.source.height = visibleHeight;
            }
        }
    }

    zoom = std::clamp(zoom, 1.0f, 4.0f);
    const float maxPan = 1.0f - 1.0f / zoom;
    panX = std::clamp(panX, -maxPan, maxPan);
    panY = std::clamp(panY, -maxPan, maxPan);

    if (zoom > 1.0f) {
        const RectF base = result.source;
        const float zoomedWidth = base.width / zoom;
        const float zoomedHeight = base.height / zoom;
        const float centeredX = base.x + (base.width - zoomedWidth) * 0.5f;
        const float centeredY = base.y + (base.height - zoomedHeight) * 0.5f;

        result.source.x = centeredX + panX * base.width * 0.5f;
        result.source.y = centeredY + panY * base.height * 0.5f;
        result.source.width = zoomedWidth;
        result.source.height = zoomedHeight;

        result.source.x = std::clamp(result.source.x,
                                     base.x,
                                     base.x + base.width - zoomedWidth);
        result.source.y = std::clamp(result.source.y,
                                     base.y,
                                     base.y + base.height - zoomedHeight);
    }

    return result;
}

} // namespace artemis::video
