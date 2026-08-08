#pragma once

#include <string>

namespace artemis::performance {

struct LitePerformanceSnapshot {
    double networkMbps = 0.0;
    double receiveLatencyMs = 0.0;
    double decodeLatencyMs = 0.0;
    double packetLossPercent = 0.0;
    double renderedFps = 0.0;
};

struct LitePerformanceStatus {
    std::string networkText;
    std::string latencyText;
    std::string decodeText;
    std::string packetLossText;
    std::string fpsText;
    bool healthy = true;
};

LitePerformanceStatus buildLiteStatus(const LitePerformanceSnapshot& snapshot);

} // namespace artemis::performance
