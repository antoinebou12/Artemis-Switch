#pragma once

namespace artemis::video {

enum class Rotation { Deg0 = 0, Deg90 = 90, Deg180 = 180, Deg270 = 270 };

struct NormalizedPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct DisplayTransform {
    Rotation rotation = Rotation::Deg0;
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
};

DisplayTransform validateDisplayTransform(DisplayTransform transform);
NormalizedPoint localToStream(NormalizedPoint point, Rotation rotation);
NormalizedPoint streamToLocal(NormalizedPoint point, Rotation rotation);
NormalizedPoint localVectorToStream(NormalizedPoint vector, Rotation rotation);
bool swapsDimensions(Rotation rotation);
Rotation nextRotation(Rotation rotation);

} // namespace artemis::video
