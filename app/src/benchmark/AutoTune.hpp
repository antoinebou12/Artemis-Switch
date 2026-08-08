#pragma once

#include <optional>
#include <string>
#include <vector>

namespace artemis::benchmark {

struct AutoTuneProfile {
    int width = 1280;
    int height = 720;
    int fps = 60;
    int bitrateKbps = 15000;
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
    int maximumBitrateKbps = 50000;
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
