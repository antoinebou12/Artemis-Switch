#pragma once

namespace artemis::video {

enum class ScaleMode { Fit, Fill, Stretch };

struct RectF {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

class VideoScale {
public:
    static RectF destinationRect(float sourceWidth, float sourceHeight,
                                 float viewportWidth, float viewportHeight,
                                 ScaleMode mode);
};

} // namespace artemis::video
