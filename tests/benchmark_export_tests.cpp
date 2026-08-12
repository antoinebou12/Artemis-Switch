#include "BenchmarkExport.hpp"
#include "BenchmarkFileStore.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

using namespace artemis::benchmark;

namespace {
std::string readAll(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
}

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

    summary.stats.sampleCount = 120;
    summary.stats.durationSeconds = 60.0;
    summary.stats.renderedFps.mean = 59.98;
    summary.stats.renderedFps.median = 60.0;
    summary.stats.renderedFps.p99 = 59.5;
    summary.stats.decodeMs.p99 = 4.2;
    summary.stats.gpuRenderMs.p99 = 1.25;
    summary.stats.queueSkippedFrames = 3;
    summary.stats.stabilityScore = 98.7;
    summary.capturedAtIso = "2026-08-11T12:00:00Z";
    summary.hostName = "Living Room PC";
    summary.appName = "Desktop";

    const std::string json = BenchmarkExport::toJson(profile, summary);
    assert(json.find("\"width\": 1920") != std::string::npos);
    assert(json.find("\"codec\": \"HEVC\"") != std::string::npos);
    assert(json.find("\"stability_score\": 98.7000") != std::string::npos);
    assert(json.find("\"runtime\"") != std::string::npos);
    assert(json.find("\"stats\"") != std::string::npos);
    assert(json.find("\"gpu_render_ms\"") != std::string::npos);
    assert(json.find("\"queue_skipped_frames\": 3") != std::string::npos);
    assert(json.find("\"captured_at\": \"2026-08-11T12:00:00Z\"") != std::string::npos);
    assert(json.find("\"host\": \"Living Room PC\"") != std::string::npos);
    assert(json.find("\"app\": \"Desktop\"") != std::string::npos);

    SwitchRuntimeMetadata runtime;
    runtime.platform = "Nintendo Switch";
    runtime.operationMode = "docked";
    runtime.batteryPercent = 83;
    runtime.batteryChargingKnown = true;
    runtime.batteryCharging = true;
    runtime.cpuClockHz = 1785000000u;
    runtime.gpuClockHz = 614000000u;
    runtime.memoryClockHz = 1862000000u;

    const std::string switchJson = BenchmarkExport::toJson(profile, summary, runtime);
    assert(switchJson.find("\"operation_mode\": \"docked\"") != std::string::npos);
    assert(switchJson.find("\"battery_percent\": 83") != std::string::npos);
    assert(switchJson.find("\"battery_charging_enabled\": true") != std::string::npos);
    assert(switchJson.find("\"cpu_clock_hz\": 1785000000") != std::string::npos);
    assert(switchJson.find("\"gpu_clock_hz\": 614000000") != std::string::npos);
    assert(switchJson.find("\"memory_clock_hz\": 1862000000") != std::string::npos);

    ExportProfile escapedProfile = profile;
    escapedProfile.codec = "HEVC\"test\nvalue";
    const std::string escapedJson = BenchmarkExport::toJson(escapedProfile, summary, runtime);
    assert(escapedJson.find("HEVC\\\"test\\nvalue") != std::string::npos);

    const std::string header = BenchmarkExport::csvHeader();
    assert(header.find("bitrate_kbps") != std::string::npos);
    assert(header.find("cpu_clock_hz") != std::string::npos);
    assert(header.find("stats_gpu_render_ms_p99") != std::string::npos);
    assert(header.find("queue_skipped_frames") != std::string::npos);
    assert(header.find("captured_at") != std::string::npos);
    assert(header.back() == '\n');

    escapedProfile.codec = "HEVC,custom";
    const std::string row = BenchmarkExport::toCsvRow(escapedProfile, summary, runtime);
    assert(row.find("1920,1080,60,20000,2,\"HEVC,custom\"") == 0);
    assert(row.find("Nintendo Switch,docked,83,true,1785000000,614000000,1862000000") !=
           std::string::npos);
    assert(row.back() == '\n');

    SwitchRuntimeMetadata unknownCharging = runtime;
    unknownCharging.batteryChargingKnown = false;
    const std::string unknownJson = BenchmarkExport::toJson(profile, summary, unknownCharging);
    assert(unknownJson.find("\"battery_charging_enabled\": null") != std::string::npos);

    const auto temp = std::filesystem::temp_directory_path() / "artemis-switch-export-test";
    std::error_code error;
    std::filesystem::remove_all(temp, error);

    const auto paths = BenchmarkFileStore::save(temp, "1080p/HEVC 20 Mbps", profile, summary);
    assert(paths.has_value());
    assert(paths->json.filename() == "1080p_HEVC_20_Mbps.json");
    assert(paths->csv.filename() == "1080p_HEVC_20_Mbps.csv");
    assert(std::filesystem::exists(paths->json));
    assert(std::filesystem::exists(paths->csv));
    assert(readAll(paths->json).find("98.7000") != std::string::npos);
    assert(readAll(paths->json).find("\"runtime\"") != std::string::npos);
    assert(readAll(paths->csv).find("width,height,fps") == 0);

    std::filesystem::remove_all(temp, error);
    return 0;
}
