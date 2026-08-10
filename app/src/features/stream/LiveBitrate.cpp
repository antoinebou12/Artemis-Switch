#include "LiveBitrate.hpp"

#include <algorithm>

namespace artemis::stream {

LiveBitratePlan planLiveBitrate(int requestedKbps,
                                bool sessionActive,
                                bool restartAlreadyInProgress) {
    return {
        std::clamp(requestedKbps, MinimumBitrateKbps, MaximumBitrateKbps),
        sessionActive && !restartAlreadyInProgress,
    };
}

} // namespace artemis::stream
