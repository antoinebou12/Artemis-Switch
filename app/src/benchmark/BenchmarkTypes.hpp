#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace artemis::benchmark {

struct BenchmarkSample {
    uint64_t timestampMs = 0;
    float hostFps = 0.0f;
    float receivedFps = 0.0f;
    float decodedFps = 0.0f;
    float renderedFps = 0.0f;
    float receiveMs = 0.0f;
    float decodeMs = 0.0f;
    float decoderDelayMs = 0.0f;
    float renderMs = 0.0f;
    float gpuRenderMs = 0.0f;
    uint64_t totalReceivedFrames = 0;
    uint64_t networkDroppedFrames = 0;
    uint64_t queueUnderflows = 0;
    uint64_t queueSkippedFrames = 0;
    uint64_t queueOverflowDrops = 0;
    uint64_t queuePacingSkips = 0;
    uint64_t queueResyncs = 0;
};

struct DistributionSummary {
    double mean = 0.0;
    double median = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double min = 0.0;
    double max = 0.0;
};

struct BenchmarkSummary {
    size_t sampleCount = 0;
    double durationSeconds = 0.0;
    DistributionSummary hostFps;
    DistributionSummary receivedFps;
    DistributionSummary decodedFps;
    DistributionSummary renderedFps;
    DistributionSummary receiveMs;
    DistributionSummary decodeMs;
    DistributionSummary decoderDelayMs;
    DistributionSummary renderMs;
    DistributionSummary gpuRenderMs;
    DistributionSummary clientProcessingMs;
    uint64_t receivedFrames = 0;
    uint64_t networkDroppedFrames = 0;
    double networkDropPercent = 0.0;
    uint64_t queueUnderflows = 0;
    uint64_t queueSkippedFrames = 0;
    uint64_t queueOverflowDrops = 0;
    uint64_t queuePacingSkips = 0;
    uint64_t queueResyncs = 0;
    double stabilityScore = 0.0;
};

struct StreamProfile {
    int width = 1280;
    int height = 720;
    int fps = 60;
    int bitrateKbps = 15000;
    int decoderThreads = 2;
    std::string codec = "HEVC";
};

struct BenchmarkRun {
    StreamProfile profile;
    BenchmarkSummary summary;
};

} // namespace artemis::benchmark
