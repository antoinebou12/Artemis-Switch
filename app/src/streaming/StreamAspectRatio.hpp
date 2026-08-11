#pragma once

#include <cstring>

namespace artemis::streaming {

enum class StreamAspectRatio {
    Ratio16x9 = 0,
    Ratio4x3 = 1,
};

inline StreamAspectRatio normalizeAspectRatio(StreamAspectRatio aspect) {
    switch (aspect) {
    case StreamAspectRatio::Ratio4x3:
        return StreamAspectRatio::Ratio4x3;
    case StreamAspectRatio::Ratio16x9:
    default:
        return StreamAspectRatio::Ratio16x9;
    }
}

inline StreamAspectRatio aspectRatioFromInt(int value) {
    return normalizeAspectRatio(static_cast<StreamAspectRatio>(value));
}

inline const char* aspectRatioToString(StreamAspectRatio aspect) {
    switch (normalizeAspectRatio(aspect)) {
    case StreamAspectRatio::Ratio4x3:
        return "4:3";
    case StreamAspectRatio::Ratio16x9:
    default:
        return "16:9";
    }
}

inline StreamAspectRatio aspectRatioFromString(const char* value) {
    if (value && (std::strcmp(value, "4:3") == 0 ||
                  std::strcmp(value, "4x3") == 0))
        return StreamAspectRatio::Ratio4x3;
    return StreamAspectRatio::Ratio16x9;
}

/** Derive stream width from a preset height for the chosen display aspect. */
inline int streamWidthFromHeight(int height, StreamAspectRatio aspect) {
    if (height <= 0)
        return 0;
    if (normalizeAspectRatio(aspect) == StreamAspectRatio::Ratio4x3)
        return height * 4 / 3;
    return height * 16 / 9;
}

} // namespace artemis::streaming
