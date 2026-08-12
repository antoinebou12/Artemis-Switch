#include "FrameColorRange.hpp"

#include <cassert>

using artemis::streaming::effectiveFrameFullRange;

int main() {
    // Forced/negotiated full wins even when NVTEGRA reports JPEG.
    assert(effectiveFrameFullRange(true, true, true));
    assert(effectiveFrameFullRange(false, true, true));

    // Limited request + NVTEGRA JPEG quirk → limited (lifted blacks fix).
    assert(!effectiveFrameFullRange(true, true, false));

    // Non-NVTEGRA trusts frame metadata.
    assert(effectiveFrameFullRange(true, false, false));
    assert(!effectiveFrameFullRange(false, false, false));

    return 0;
}
