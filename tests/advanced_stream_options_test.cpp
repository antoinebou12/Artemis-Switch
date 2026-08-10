#include "../app/src/features/stream/AdvancedStreamOptions.hpp"

#include <cassert>

using artemis::stream::AdvancedStreamOptions;
using artemis::stream::availableFrameRates;
using artemis::stream::normalizeFrameRate;

int main() {
    AdvancedStreamOptions options;
    assert((availableFrameRates(options) ==
            std::vector<int>{30, 40, 60, 90, 120}));
    assert(normalizeFrameRate(90, options) == 90);
    assert(normalizeFrameRate(92, options) == 90);
    assert(normalizeFrameRate(118, options) == 120);

    // Legacy unlock flag must not change the exposed list.
    options.unlockAllFrameRates = false;
    assert((availableFrameRates(options) ==
            std::vector<int>{30, 40, 60, 90, 120}));
    options.unlockAllFrameRates = true;
    assert((availableFrameRates(options) ==
            std::vector<int>{30, 40, 60, 90, 120}));

    options.forceFullRangeVideo = true;
    options.preventPacketLoss = true;
    assert(options.forceFullRangeVideo);
    assert(options.preventPacketLoss);
    return 0;
}
