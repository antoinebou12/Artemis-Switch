#include "../app/src/features/stream/AdvancedStreamOptions.hpp"

#include <cassert>
#include <vector>

using artemis::stream::AdvancedStreamOptions;
using artemis::stream::availableFrameRates;
using artemis::stream::clampPacketSize;
using artemis::stream::kPacketSizeAuto;
using artemis::stream::kPacketSizeDefault;
using artemis::stream::kPacketSizeMax;
using artemis::stream::kPacketSizeMin;
using artemis::stream::kPacketSizeVpnFriendly;
using artemis::stream::normalizeFrameRate;
using artemis::stream::resolveStreamPacketSize;

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

    assert(clampPacketSize(kPacketSizeAuto) == kPacketSizeAuto);
    assert(clampPacketSize(-10) == kPacketSizeAuto);
    assert(clampPacketSize(100) == kPacketSizeMin);
    assert(clampPacketSize(70000) == kPacketSizeMax);
    assert(clampPacketSize(1346) == 1346);

    // Auto + no guard → default LAN size.
    assert(resolveStreamPacketSize(kPacketSizeAuto, false) ==
           kPacketSizeDefault);
    // Auto + packet-loss guard → VPN-friendly size.
    assert(resolveStreamPacketSize(kPacketSizeAuto, true) ==
           kPacketSizeVpnFriendly);
    // Explicit default + guard still lowers to VPN-friendly.
    assert(resolveStreamPacketSize(kPacketSizeDefault, true) ==
           kPacketSizeVpnFriendly);
    // Explicit custom size wins even with the guard.
    assert(resolveStreamPacketSize(1346, true) == 1346);
    assert(resolveStreamPacketSize(1024, false) == 1024);

    options.packetSize = kPacketSizeAuto;
    options.preventPacketLoss = false;
    assert(resolveStreamPacketSize(options) == kPacketSizeDefault);
    options.preventPacketLoss = true;
    assert(resolveStreamPacketSize(options) == kPacketSizeVpnFriendly);
    return 0;
}
