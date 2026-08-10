#include "DisplayTransform.hpp"

#include <algorithm>

namespace artemis::video {

DisplayTransform validateDisplayTransform(DisplayTransform transform) {
    transform.zoom = std::clamp(transform.zoom, 1.0f, 4.0f);
    const float maxPan = 1.0f - 1.0f / transform.zoom;
    transform.panX = std::clamp(transform.panX, -maxPan, maxPan);
    transform.panY = std::clamp(transform.panY, -maxPan, maxPan);
    return transform;
}

NormalizedPoint localToStream(NormalizedPoint p, Rotation rotation) {
    switch (rotation) {
    case Rotation::Deg90: return {p.y, 1.0f - p.x};
    case Rotation::Deg180: return {1.0f - p.x, 1.0f - p.y};
    case Rotation::Deg270: return {1.0f - p.y, p.x};
    case Rotation::Deg0: default: return p;
    }
}

NormalizedPoint streamToLocal(NormalizedPoint p, Rotation rotation) {
    switch (rotation) {
    case Rotation::Deg90: return {1.0f - p.y, p.x};
    case Rotation::Deg180: return {1.0f - p.x, 1.0f - p.y};
    case Rotation::Deg270: return {p.y, 1.0f - p.x};
    case Rotation::Deg0: default: return p;
    }
}

NormalizedPoint localVectorToStream(NormalizedPoint p, Rotation rotation) {
    switch (rotation) {
    case Rotation::Deg90: return {p.y, -p.x};
    case Rotation::Deg180: return {-p.x, -p.y};
    case Rotation::Deg270: return {-p.y, p.x};
    case Rotation::Deg0: default: return p;
    }
}

bool swapsDimensions(Rotation rotation) {
    return rotation == Rotation::Deg90 || rotation == Rotation::Deg270;
}

Rotation nextRotation(Rotation rotation) {
    switch (rotation) {
    case Rotation::Deg0: return Rotation::Deg90;
    case Rotation::Deg90: return Rotation::Deg180;
    case Rotation::Deg180: return Rotation::Deg270;
    case Rotation::Deg270: default: return Rotation::Deg0;
    }
}

} // namespace artemis::video
