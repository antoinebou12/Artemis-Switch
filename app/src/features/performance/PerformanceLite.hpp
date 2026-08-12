#pragma once

#include <string>

namespace artemis::performance {

struct LitePerformanceSnapshot {
    double networkMbps = 0.0;
    double configuredMbps = 0.0;
    double receiveLatencyMs = 0.0;
    double decodeLatencyMs = 0.0;
    double renderLatencyMs = 0.0;
    double queueWaitMs = 0.0;
    double clientPipelineMs = 0.0;
    double queueJitterMs = 0.0;
    double packetLossPercent = 0.0;
    double hostFps = 0.0;
    double receivedFps = 0.0;
    double decodedFps = 0.0;
    double renderedFps = 0.0;
    double gpuRenderMs = 0.0;
    double postProcessMs = 0.0;
    double fsrMs = 0.0;
    double rcasMs = 0.0;
    double ditherMs = 0.0;
    int queueDepth = 0;
    int queueTarget = 0;
    int queueCapacity = 0;
    std::string presentationMode = "Fill";
    std::string colorRange = "Limited";
};

struct LitePerformanceStatus {
    std::string networkText;
    std::string latencyText;
    std::string decodeText;
    std::string renderText;
    std::string packetLossText;
    std::string hostFpsText;
    std::string receivedFpsText;
    std::string decodedFpsText;
    std::string fpsText;
    std::string gpuText;
    std::string postProcessText;
    std::string frameQueueText;
    std::string presentationText;
    bool healthy = true;
};

LitePerformanceStatus buildLiteStatus(const LitePerformanceSnapshot& snapshot);
float normalizeWifiSignal(int signalLevel);

} // namespace artemis::performance
