#include "PerformanceLite.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace artemis::performance {
namespace {
std::string fixed(double value, int precision, const char* suffix) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value << suffix;
    return out.str();
}
}

LitePerformanceStatus buildLiteStatus(const LitePerformanceSnapshot& snapshot) {
    LitePerformanceStatus status;
    status.networkText = fixed(std::max(0.0, snapshot.networkMbps), 1, " Mbps");
    status.latencyText = fixed(std::max(0.0, snapshot.receiveLatencyMs), 2, " ms");
    status.decodeText = fixed(std::max(0.0, snapshot.decodeLatencyMs), 2, " ms");
    status.packetLossText = fixed(std::clamp(snapshot.packetLossPercent, 0.0, 100.0), 2, "%");
    status.fpsText = fixed(std::max(0.0, snapshot.renderedFps), 2, " FPS");

    // Switch-first health thresholds. They are intentionally conservative and
    // only drive UI state; Auto Tune performs the stricter profile ranking.
    status.healthy = snapshot.packetLossPercent <= 0.10 &&
                     snapshot.receiveLatencyMs <= 8.0 &&
                     snapshot.decodeLatencyMs <= 12.0 &&
                     snapshot.renderedFps >= 55.0;
    return status;
}

} // namespace artemis::performance
