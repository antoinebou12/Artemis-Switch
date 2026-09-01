#include "BitrateSteps.hpp"

#include <algorithm>
#include <cmath>

namespace artemis::stream {
namespace {

constexpr int kFineStepKbps = 500;
constexpr int kCoarseStepKbps = 1000;
constexpr int kCoarseThresholdKbps = 20000;

int stepForKbps(int kbps) {
    return kbps < kCoarseThresholdKbps ? kFineStepKbps : kCoarseStepKbps;
}

} // namespace

BitrateSliderRange bitrateSliderRange() {
#if defined(__PSV__)
    return {500, 20000};
#elif defined(PLATFORM_SWITCH)
    return {500, 100000};
#else
    return {500, 150000};
#endif
}

int quantizeBitrateKbps(int kbps, BitrateSliderRange range) {
    if (range.maxKbps <= range.minKbps)
        return range.minKbps;

    const int clamped = std::clamp(kbps, range.minKbps, range.maxKbps);
    const int step = stepForKbps(clamped);
    // The grid is anchored at zero, not at the range minimum, so the fine and
    // coarse grids agree at the 20 Mbps boundary. Anchoring at the minimum
    // instead would make 20000 snap to 20500, and quantizing would not be
    // idempotent across the boundary.
    const int snapped = ((clamped + step / 2) / step) * step;
    return std::clamp(snapped, range.minKbps, range.maxKbps);
}

int bitrateFromSliderProgress(float progress, BitrateSliderRange range) {
    if (range.maxKbps <= range.minKbps)
        return range.minKbps;

    const float clamped = std::clamp(progress, 0.0f, 1.0f);
    const float span = static_cast<float>(range.maxKbps - range.minKbps);
    const int raw =
        range.minKbps + static_cast<int>(std::lround(clamped * span));
    return quantizeBitrateKbps(raw, range);
}

float sliderProgressFromBitrate(int kbps, BitrateSliderRange range) {
    if (range.maxKbps <= range.minKbps)
        return 0.0f;

    const int clamped = std::clamp(kbps, range.minKbps, range.maxKbps);
    const float span = static_cast<float>(range.maxKbps - range.minKbps);
    return std::clamp(static_cast<float>(clamped - range.minKbps) / span, 0.0f,
                      1.0f);
}

} // namespace artemis::stream
