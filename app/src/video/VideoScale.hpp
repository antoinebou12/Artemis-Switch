#pragma once

namespace artemis::video {

enum class ScaleMode { Fit, Fill, Stretch };

struct RectF {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct PresentationGeometry {
    RectF source;
    RectF destination;
};

class VideoScale {
public:
    static RectF destinationRect(float sourceWidth, float sourceHeight,
                                 float viewportWidth, float viewportHeight,
                                 ScaleMode mode);

    // Returns both the source crop and destination viewport used by the real
    // renderer. panX/panY are normalized offsets and zoom is clamped to 1x-4x.
    static PresentationGeometry presentationGeometry(
        float sourceWidth, float sourceHeight,
        float viewportWidth, float viewportHeight,
        ScaleMode mode, float zoom = 1.0f,
        float panX = 0.0f, float panY = 0.0f);
};

} // namespace artemis::video
