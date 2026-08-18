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
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(1)
            << std::max(0.0, snapshot.networkMbps) << " / "
            << std::max(0.0, snapshot.configuredMbps) << " Mbps";
        status.networkText = out.str();
    }
    status.receiveText =
        fixed(std::max(0.0, snapshot.receiveLatencyMs), 2, " ms");
    status.decodeText = fixed(std::max(0.0, snapshot.decodeLatencyMs), 2, " ms");
    status.renderText = fixed(std::max(0.0, snapshot.renderLatencyMs), 2, " ms");
    status.packetLossText = fixed(std::clamp(snapshot.packetLossPercent, 0.0, 100.0), 2, "%");
    status.hostFpsText = fixed(std::max(0.0, snapshot.hostFps), 2, " FPS");
    status.receivedFpsText = fixed(std::max(0.0, snapshot.receivedFps), 2, " FPS");
    status.decodedFpsText = fixed(std::max(0.0, snapshot.decodedFps), 2, " FPS");
    status.fpsText = fixed(std::max(0.0, snapshot.renderedFps), 2, " FPS");
    status.gpuText = fixed(std::max(0.0, snapshot.gpuRenderMs), 2, " ms");
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(2)
            << std::max(0.0, snapshot.postProcessMs) << " ms"
            << " (FSR " << std::max(0.0, snapshot.fsrMs)
            << " / RCAS " << std::max(0.0, snapshot.rcasMs)
            << " / D " << std::max(0.0, snapshot.ditherMs) << ")";
        status.postProcessText = out.str();
    }
    {
        std::ostringstream out;
        out << snapshot.queueDepth << " / " << snapshot.queueTarget;
        if (snapshot.queueJitterMs > 0.0) {
            out << std::fixed << std::setprecision(1) << " · j "
                << snapshot.queueJitterMs << " ms";
        }
        status.frameQueueText = out.str();
    }
    status.presentationText =
        snapshot.presentationMode + " · " + snapshot.colorRange;

    status.healthy = snapshot.packetLossPercent <= 0.10 &&
                     snapshot.receiveLatencyMs <= 8.0 &&
                     snapshot.decodeLatencyMs <= 12.0 &&
                     snapshot.renderedFps >= 55.0;
    return status;
}

float normalizeWifiSignal(int signalLevel) {
    return static_cast<float>(std::clamp(signalLevel, 0, 3)) / 3.0f;
}

} // namespace artemis::performance
