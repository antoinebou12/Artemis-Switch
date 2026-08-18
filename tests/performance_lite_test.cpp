#include "../app/src/features/performance/PerformanceLite.hpp"

#include <cassert>
#include <string>

using artemis::performance::LitePerformanceSnapshot;
using artemis::performance::buildLiteStatus;
using artemis::performance::normalizeWifiSignal;

int main() {
    {
        LitePerformanceSnapshot snapshot;
        snapshot.networkMbps = 2.8;
        snapshot.configuredMbps = 4.9;
        snapshot.receiveLatencyMs = 0.8;
        snapshot.decodeLatencyMs = 2.6;
        snapshot.renderLatencyMs = 0.7;
        snapshot.queueWaitMs = 1.9;
        snapshot.clientPipelineMs = 6.0;
        snapshot.queueJitterMs = 1.3;
        snapshot.gpuRenderMs = 1.5;
        snapshot.packetLossPercent = 0.02;
        snapshot.hostFps = 60.0;
        snapshot.receivedFps = 59.9;
        snapshot.decodedFps = 59.8;
        snapshot.renderedFps = 59.98;
        snapshot.queueDepth = 0;
        snapshot.queueTarget = 1;
        snapshot.queueCapacity = 8;
        snapshot.presentationMode = "Fill";
        snapshot.colorRange = "Full";
        const auto status = buildLiteStatus(snapshot);
        assert(status.networkText == "2.8 / 4.9 Mbps");
        assert(status.receiveText == "0.80 ms");
        assert(status.decodeText == "2.60 ms");
        assert(status.renderText == "0.70 ms");
        assert(status.gpuText == "1.50 ms");
        assert(status.packetLossText == "0.02%");
        assert(status.fpsText == "59.98 FPS");
        assert(status.frameQueueText == "0 / 1 · j 1.3 ms");
        assert(status.presentationText == "Fill · Full");
        assert(status.healthy);
    }

    {
        LitePerformanceSnapshot snapshot;
        snapshot.receiveLatencyMs = 12.0;
        snapshot.renderedFps = 60.0;
        assert(!buildLiteStatus(snapshot).healthy);
    }

    {
        LitePerformanceSnapshot snapshot;
        snapshot.receiveLatencyMs = 2.0;
        snapshot.packetLossPercent = 1.5;
        snapshot.renderedFps = 60.0;
        assert(!buildLiteStatus(snapshot).healthy);
    }

    assert(normalizeWifiSignal(-1) == 0.0f);
    assert(normalizeWifiSignal(0) == 0.0f);
    assert(normalizeWifiSignal(1) > 0.33f && normalizeWifiSignal(1) < 0.34f);
    assert(normalizeWifiSignal(2) > 0.66f && normalizeWifiSignal(2) < 0.67f);
    assert(normalizeWifiSignal(3) == 1.0f);
    assert(normalizeWifiSignal(10) == 1.0f);

    return 0;
}
