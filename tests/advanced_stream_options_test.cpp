#include "../app/src/features/stream/AdvancedStreamOptions.hpp"

#include <cassert>

using artemis::stream::AdvancedStreamOptions;
using artemis::stream::availableFrameRates;
using artemis::stream::normalizeFrameRate;

int main() {
    AdvancedStreamOptions locked;
    assert((availableFrameRates(locked) == std::vector<int>{30, 40, 60}));
    assert(normalizeFrameRate(90, locked) == 60);

    AdvancedStreamOptions unlocked;
    unlocked.unlockAllFrameRates = true;
    assert((availableFrameRates(unlocked) == std::vector<int>{30, 40, 60, 90, 120}));
    assert(normalizeFrameRate(92, unlocked) == 90);
    assert(normalizeFrameRate(118, unlocked) == 120);

    unlocked.forceFullRangeVideo = true;
    unlocked.preventPacketLoss = true;
    assert(unlocked.forceFullRangeVideo);
    assert(unlocked.preventPacketLoss);
    return 0;
}
