#pragma once

#include <vector>

namespace artemis::stream {

struct AdvancedStreamOptions {
    bool unlockAllFrameRates = false;
    bool forceFullRangeVideo = false;
    bool preventPacketLoss = false;
};

std::vector<int> availableFrameRates(const AdvancedStreamOptions& options);
int normalizeFrameRate(int requestedFps, const AdvancedStreamOptions& options);

} // namespace artemis::stream
