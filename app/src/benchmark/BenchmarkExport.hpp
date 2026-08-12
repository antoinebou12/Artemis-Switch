#pragma once

#include "BenchmarkTypes.hpp"
#include "SwitchRuntimeMetadata.hpp"
#include <string>

namespace artemis::benchmark {

struct ExportProfile {
    int width = 1280;
    int height = 720;
    int fps = 60;
    int bitrateKbps = 15000;
    int decoderThreads = 2;
    std::string codec = "HEVC";
};

struct ExportSummary {
    double durationSeconds = 0.0;
    double renderedFpsMean = 0.0;
    double renderedFpsP99 = 0.0;
    double networkDropPercent = 0.0;
    double receiveMsP99 = 0.0;
    double decodeMsP99 = 0.0;
    double clientProcessingMsP99 = 0.0;
    double stabilityScore = 0.0;
    BenchmarkSummary stats;
    std::string capturedAtIso;
    std::string hostName;
    std::string appName;
};

inline ExportSummary fromBenchmarkSummary(const BenchmarkSummary& summary) {
    ExportSummary result;
    result.durationSeconds = summary.durationSeconds;
    result.renderedFpsMean = summary.renderedFps.mean;
    result.renderedFpsP99 = summary.renderedFps.p99;
    result.networkDropPercent = summary.networkDropPercent;
    result.receiveMsP99 = summary.receiveMs.p99;
    result.decodeMsP99 = summary.decodeMs.p99;
    result.clientProcessingMsP99 = summary.clientProcessingMs.p99;
    result.stabilityScore = summary.stabilityScore;
    result.stats = summary;
    return result;
}

class BenchmarkExport {
public:
    // Convenience overloads collect a fresh runtime snapshot.
    static std::string toJson(const ExportProfile& profile,
                              const ExportSummary& summary);
    static std::string toCsvRow(const ExportProfile& profile,
                                const ExportSummary& summary);

    // File export uses these overloads so JSON and CSV share exactly the same
    // Switch state/clock snapshot.
    static std::string toJson(const ExportProfile& profile,
                              const ExportSummary& summary,
                              const SwitchRuntimeMetadata& runtime);
    static std::string csvHeader();
    static std::string toCsvRow(const ExportProfile& profile,
                                const ExportSummary& summary,
                                const SwitchRuntimeMetadata& runtime);
};

} // namespace artemis::benchmark
