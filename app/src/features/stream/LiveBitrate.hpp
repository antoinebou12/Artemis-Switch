#pragma once

namespace artemis::stream {

constexpr int MinimumBitrateKbps = 1000;
constexpr int MaximumBitrateKbps = 100000;

struct LiveBitratePlan {
    int bitrateKbps = MinimumBitrateKbps;
    bool restartActiveStream = false;
};

LiveBitratePlan planLiveBitrate(int requestedKbps,
                                bool sessionActive,
                                bool restartAlreadyInProgress);

} // namespace artemis::stream
