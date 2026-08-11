#include "AdvancedStreamOptions.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace artemis::stream {

std::vector<int> availableFrameRates(const AdvancedStreamOptions& /*options*/) {
    // Always expose the full Switch stream profile set (no unlock gate).
    return {30, 40, 60, 90, 120};
}

int normalizeFrameRate(int requestedFps, const AdvancedStreamOptions& options) {
    const auto rates = availableFrameRates(options);
    return *std::min_element(rates.begin(), rates.end(), [requestedFps](int lhs, int rhs) {
        return std::abs(lhs - requestedFps) < std::abs(rhs - requestedFps);
    });
}

int clampPacketSize(int packetSize) {
    if (packetSize <= kPacketSizeAuto)
        return kPacketSizeAuto;
    return std::clamp(packetSize, kPacketSizeMin, kPacketSizeMax);
}

int resolveStreamPacketSize(int packetSize, bool preventPacketLoss) {
    const int clamped = clampPacketSize(packetSize);
    if (clamped == kPacketSizeAuto) {
        return preventPacketLoss ? kPacketSizeVpnFriendly : kPacketSizeDefault;
    }
    // Packet-loss guard only overrides the common default; explicit sizes win.
    if (preventPacketLoss && clamped == kPacketSizeDefault)
        return kPacketSizeVpnFriendly;
    return clamped;
}

} // namespace artemis::stream
