#include "BenchmarkExport.hpp"

#include <cassert>
#include <string>

using namespace artemis::benchmark;

int main() {
    ExportProfile profile{1920, 1080, 60, 20000, 2, "HEVC"};
    ExportSummary summary;
    summary.durationSeconds = 60.0;
    summary.renderedFpsMean = 59.98;
    summary.renderedFpsP99 = 59.5;
    summary.networkDropPercent = 0.01;
    summary.receiveMsP99 = 2.1;
    summary.decodeMsP99 = 4.2;
    summary.clientProcessingMsP99 = 6.3;
    summary.stabilityScore = 98.7;

    const std::string json = BenchmarkExport::toJson(profile, summary);
    assert(json.find("\"width\": 1920") != std::string::npos);
    assert(json.find("\"codec\": \"HEVC\"") != std::string::npos);
    assert(json.find("\"stability_score\": 98.7000") != std::string::npos);

    const std::string header = BenchmarkExport::csvHeader();
    assert(header.find("bitrate_kbps") != std::string::npos);
    assert(header.back() == '\n');

    const std::string row = BenchmarkExport::toCsvRow(profile, summary);
    assert(row.find("1920,1080,60,20000,2,HEVC") == 0);
    assert(row.back() == '\n');

    return 0;
}
