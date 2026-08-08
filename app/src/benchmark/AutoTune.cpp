#include "AutoTune.hpp"

#include <algorithm>
#include <cmath>

namespace artemis::benchmark {

bool AutoTune::isStable(const AutoTuneResult& result, const AutoTunePolicy& policy) {
    const int targetFps = std::max(result.profile.fps, 1);
    const double fpsRatio = result.renderedFpsMean / static_cast<double>(targetFps);
    return result.stabilityScore >= policy.minimumStabilityScore &&
           result.networkDropPercent <= policy.maximumNetworkDropPercent &&
           fpsRatio >= policy.minimumRenderedFpsRatio;
}

double AutoTune::qualityScore(const AutoTuneResult& result,
                              const AutoTunePolicy& policy) {
    if (!isStable(result, policy)) return -1.0;
    if (result.profile.height > policy.maximumHeight ||
        result.profile.bitrateKbps > policy.maximumBitrateKbps)
        return -1.0;

    const double pixels = static_cast<double>(result.profile.width) *
                          static_cast<double>(result.profile.height);
    const double resolutionScore = std::log2(std::max(pixels, 1.0)) * 10.0;
    const double bitrateScore = std::log2(std::max(result.profile.bitrateKbps, 1000) / 1000.0) * 4.0;
    const double fpsScore = static_cast<double>(result.profile.fps) * 0.15;
    const double threadPenalty = std::max(0, result.profile.decoderThreads - 2) * 0.5;
    const double latencyPenalty = std::max(0.0, result.clientP99Ms - 8.0) * 0.4;

    return resolutionScore + bitrateScore + fpsScore +
           result.stabilityScore * 0.25 - threadPenalty - latencyPenalty;
}

std::optional<AutoTuneResult>
AutoTune::chooseBest(const std::vector<AutoTuneResult>& results,
                     const AutoTunePolicy& policy) {
    const AutoTuneResult* best = nullptr;
    double bestScore = -1.0;

    for (const auto& result : results) {
        const double score = qualityScore(result, policy);
        if (score > bestScore) {
            bestScore = score;
            best = &result;
        }
    }

    if (!best) return std::nullopt;
    return *best;
}

} // namespace artemis::benchmark
