#include "AdvancedStreamOptions.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace artemis::stream {

std::vector<int> availableFrameRates(const AdvancedStreamOptions& options) {
    if (options.unlockAllFrameRates)
        return {30, 40, 60, 90, 120};
    return {30, 40, 60};
}

int normalizeFrameRate(int requestedFps, const AdvancedStreamOptions& options) {
    const auto rates = availableFrameRates(options);
    return *std::min_element(rates.begin(), rates.end(), [requestedFps](int lhs, int rhs) {
        return std::abs(lhs - requestedFps) < std::abs(rhs - requestedFps);
    });
}

} // namespace artemis::stream
