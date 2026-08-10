#include "AdvancedStreamOptions.hpp"

#include <algorithm>
#include <cmath>

namespace artemis::stream {

std::vector<int> availableFrameRates(const AdvancedStreamOptions&) {
    // Keep the complete profile list visible. 90/120 FPS are explicit user
    // choices rather than a second hidden/unlock setting that duplicates the
    // frame-rate selector.
    return {30, 40, 60, 90, 120};
}

int normalizeFrameRate(int requestedFps, const AdvancedStreamOptions& options) {
    const auto rates = availableFrameRates(options);
    return *std::min_element(rates.begin(), rates.end(), [requestedFps](int lhs, int rhs) {
        return std::abs(lhs - requestedFps) < std::abs(rhs - requestedFps);
    });
}

} // namespace artemis::stream
