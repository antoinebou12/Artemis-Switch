#include "../app/src/features/stream/LiveBitrate.hpp"

#include <cassert>

using namespace artemis::stream;

int main() {
    auto plan = planLiveBitrate(20000, true, false);
    assert(plan.bitrateKbps == 20000);
    assert(plan.restartActiveStream);

    plan = planLiveBitrate(100, true, false);
    assert(plan.bitrateKbps == MinimumBitrateKbps);

    plan = planLiveBitrate(500000, true, false);
    assert(plan.bitrateKbps == MaximumBitrateKbps);

    plan = planLiveBitrate(15000, false, false);
    assert(!plan.restartActiveStream);

    plan = planLiveBitrate(15000, true, true);
    assert(!plan.restartActiveStream);
    return 0;
}
