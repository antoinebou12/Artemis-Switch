#include "../app/src/features/stream/BitrateSteps.hpp"

#include <cassert>

using artemis::stream::bitrateFromSliderProgress;
using artemis::stream::BitrateSliderRange;
using artemis::stream::bitrateSliderRange;
using artemis::stream::quantizeBitrateKbps;
using artemis::stream::sliderProgressFromBitrate;

int main() {
    // The Switch range, pinned so the test does not depend on build platform.
    const BitrateSliderRange nx{500, 100000};

    // Fine steps below 20 Mbps.
    assert(quantizeBitrateKbps(10347, nx) == 10500);
    assert(quantizeBitrateKbps(10000, nx) == 10000);
    assert(quantizeBitrateKbps(10200, nx) == 10000);
    assert(quantizeBitrateKbps(10300, nx) == 10500);

    // Coarse steps at and above 20 Mbps.
    assert(quantizeBitrateKbps(35400, nx) == 35000);
    assert(quantizeBitrateKbps(35600, nx) == 36000);
    assert(quantizeBitrateKbps(36000, nx) == 36000);

    // The fine and coarse grids meet at the boundary, so quantizing there is
    // stable in both directions.
    assert(quantizeBitrateKbps(20000, nx) == 20000);
    assert(quantizeBitrateKbps(19900, nx) == 20000);
    assert(quantizeBitrateKbps(20200, nx) == 20000);

    // The bounds stay exactly reachable.
    assert(quantizeBitrateKbps(0, nx) == nx.minKbps);
    assert(quantizeBitrateKbps(500, nx) == nx.minKbps);
    assert(quantizeBitrateKbps(999999, nx) == nx.maxKbps);

    // Slider ends map to the bounds.
    assert(bitrateFromSliderProgress(0.0f, nx) == nx.minKbps);
    assert(bitrateFromSliderProgress(1.0f, nx) == nx.maxKbps);
    assert(bitrateFromSliderProgress(-1.0f, nx) == nx.minKbps);
    assert(bitrateFromSliderProgress(2.0f, nx) == nx.maxKbps);

    // Every slider position lands on a step.
    for (int i = 0; i <= 1000; ++i) {
        const int kbps = bitrateFromSliderProgress(i / 1000.0f, nx);
        assert(kbps >= nx.minKbps && kbps <= nx.maxKbps);
        assert(quantizeBitrateKbps(kbps, nx) == kbps);
    }

    // A quantized bitrate round-trips through the slider unchanged, which is
    // what keeps the restored slider position from drifting on reopen.
    const int samples[] = {500, 1000, 5000, 10000, 19500, 20000, 50000, 100000};
    for (int kbps : samples) {
        const float progress = sliderProgressFromBitrate(kbps, nx);
        assert(bitrateFromSliderProgress(progress, nx) == kbps);
    }

    // Progress is monotonic and clamped.
    assert(sliderProgressFromBitrate(0, nx) == 0.0f);
    assert(sliderProgressFromBitrate(999999, nx) == 1.0f);
    assert(sliderProgressFromBitrate(10000, nx) <
           sliderProgressFromBitrate(20000, nx));

    // A degenerate range collapses to its minimum instead of dividing by zero.
    const BitrateSliderRange degenerate{500, 500};
    assert(quantizeBitrateKbps(9000, degenerate) == 500);
    assert(bitrateFromSliderProgress(0.5f, degenerate) == 500);
    assert(sliderProgressFromBitrate(9000, degenerate) == 0.0f);

    // The platform range is self-consistent.
    const BitrateSliderRange platform = bitrateSliderRange();
    assert(platform.minKbps < platform.maxKbps);
    assert(quantizeBitrateKbps(platform.maxKbps, platform) == platform.maxKbps);

    return 0;
}
