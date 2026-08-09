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

    // Resolution remains valuable, but on Switch a small increase in client-side
    // latency is much more noticeable than a small increase in video quality.
    const double resolutionScore = std::log2(std::max(pixels, 1.0)) * 3.0;
    const double fpsScore = static_cast<double>(result.profile.fps) * 0.10;
    const double stabilityScore = result.stabilityScore * 0.35;
    const double threadPenalty = std::max(0, result.profile.decoderThreads - 2) * 0.75;

    const double latencyPenalty =
        std::max(0.0, result.clientP99Ms - policy.preferredClientP99Ms) *
        policy.latencyPenaltyPerMs;

    const double excessBitrateMbps =
        std::max(0, result.profile.bitrateKbps - policy.lowLatencyBitrateCeilingKbps) /
        1000.0;
    const double excessBitratePenalty =
        excessBitrateMbps * policy.excessBitratePenaltyPerMbps;

    // Inside the measured 5-7 Mbps region, use closeness to the preferred
    // bitrate only as a tie-breaker. Never reward bitrate simply for being high.
    const double bitrateDistanceMbps =
        std::abs(result.profile.bitrateKbps - policy.preferredBitrateKbps) / 1000.0;
    const double bitrateTieBreakPenalty = bitrateDistanceMbps * 0.10;

    return resolutionScore + fpsScore + stabilityScore - threadPenalty -
           latencyPenalty - excessBitratePenalty - bitrateTieBreakPenalty;
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
