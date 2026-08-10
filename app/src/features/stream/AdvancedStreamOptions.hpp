#pragma once

#include <vector>

namespace artemis::stream {

struct AdvancedStreamOptions {
    // Kept for backward-compatible JSON load; ignored by availableFrameRates().
    bool unlockAllFrameRates = false;
    bool forceFullRangeVideo = false;
    bool preventPacketLoss = false;
};

std::vector<int> availableFrameRates(const AdvancedStreamOptions& options);
int normalizeFrameRate(int requestedFps, const AdvancedStreamOptions& options);

} // namespace artemis::stream
