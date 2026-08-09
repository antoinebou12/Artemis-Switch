#pragma once

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
};

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
