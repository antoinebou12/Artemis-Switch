#pragma once

#include <vector>

namespace artemis::stream {

// Match Sunshine packetsize docs (0 = Auto on the client).
constexpr int kPacketSizeAuto = 0;
constexpr int kPacketSizeMin = 200;
constexpr int kPacketSizeMax = 65535;
constexpr int kPacketSizeDefault = 1392;
constexpr int kPacketSizeVpnFriendly = 1024;

struct AdvancedStreamOptions {
    // Kept for backward-compatible JSON load; ignored by availableFrameRates().
    bool unlockAllFrameRates = false;
    bool forceFullRangeVideo = false;
    bool preventPacketLoss = false;
    // 0 = Auto (1392, or 1024 when preventPacketLoss is on).
    int packetSize = kPacketSizeAuto;
};

std::vector<int> availableFrameRates(const AdvancedStreamOptions& options);
int normalizeFrameRate(int requestedFps, const AdvancedStreamOptions& options);

// Keep Auto (0); otherwise clamp into [kPacketSizeMin, kPacketSizeMax].
int clampPacketSize(int packetSize);

// Resolve the value written into STREAM_CONFIGURATION::packetSize.
int resolveStreamPacketSize(int packetSize, bool preventPacketLoss);
inline int resolveStreamPacketSize(const AdvancedStreamOptions& options) {
    return resolveStreamPacketSize(options.packetSize, options.preventPacketLoss);
}

} // namespace artemis::stream
