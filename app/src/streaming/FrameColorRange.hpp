#pragma once

namespace artemis::streaming {

// Decide effective full-range for YUV→RGB.
// NVTEGRA often reports JPEG even when limited was negotiated; only apply that
// quirk when the host request was limited. Force-full-range and negotiated full
// must win.
inline bool effectiveFrameFullRange(bool frameReportsJpegRange,
                                    bool isNvtegraFormat,
                                    bool negotiatedOrForcedFullRange) {
    if (negotiatedOrForcedFullRange)
        return true;
    if (isNvtegraFormat && frameReportsJpegRange)
        return false;
    return frameReportsJpegRange;
}

} // namespace artemis::streaming
