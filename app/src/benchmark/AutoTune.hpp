#pragma once

#include <optional>
#include <string>
#include <vector>

namespace artemis::benchmark {

struct AutoTuneProfile {
    int width = 1280;
    int height = 720;
    int fps = 60;
    int bitrateKbps = 6000;
    int decoderThreads = 2;
    std::string codec = "HEVC";
};

struct AutoTuneResult {
    AutoTuneProfile profile;
    double stabilityScore = 0.0;
    double networkDropPercent = 0.0;
    double renderedFpsMean = 0.0;
    double clientP99Ms = 0.0;
};

struct AutoTunePolicy {
    double minimumStabilityScore = 95.0;
    double maximumNetworkDropPercent = 0.10;
    double minimumRenderedFpsRatio = 0.995;
    int maximumHeight = 1080;
    int maximumBitrateKbps = 20000;

    // Switch OLED measurements show a useful low-latency operating region around
    // 720p60 at 5-7 Mbps. Treat latency as the primary objective and only spend
    // additional bitrate when measurements show it is effectively free.
    int preferredBitrateKbps = 6000;
    int lowLatencyBitrateCeilingKbps = 7000;
    double preferredClientP99Ms = 6.0;
    double latencyPenaltyPerMs = 4.0;
    double excessBitratePenaltyPerMbps = 1.5;
};

class AutoTune {
public:
    static bool isStable(const AutoTuneResult& result,
                         const AutoTunePolicy& policy = {});
    static double qualityScore(const AutoTuneResult& result,
                               const AutoTunePolicy& policy = {});
    static std::optional<AutoTuneResult>
    chooseBest(const std::vector<AutoTuneResult>& results,
               const AutoTunePolicy& policy = {});
};

} // namespace artemis::benchmark
