#include "../app/src/features/stream/AdvancedStreamOptions.hpp"

#include <cassert>

using artemis::stream::AdvancedStreamOptions;
using artemis::stream::availableFrameRates;
using artemis::stream::normalizeFrameRate;

int main() {
    AdvancedStreamOptions options;
    assert((availableFrameRates(options) == std::vector<int>{30, 40, 60, 90, 120}));
    assert(normalizeFrameRate(92, options) == 90);
    assert(normalizeFrameRate(118, options) == 120);

    // Keep the old persisted flag backward-compatible, but it no longer hides
    // 90/120 FPS from the selector.
    options.unlockAllFrameRates = true;
    assert((availableFrameRates(options) == std::vector<int>{30, 40, 60, 90, 120}));

    options.forceFullRangeVideo = true;
    options.preventPacketLoss = true;
    assert(options.forceFullRangeVideo);
    assert(options.preventPacketLoss);
    return 0;
}
