#include "../app/src/features/performance/PerformanceLite.hpp"

#include <cassert>

using artemis::performance::LitePerformanceSnapshot;
using artemis::performance::buildLiteStatus;
using artemis::performance::normalizeWifiSignal;

int main() {
    {
        const auto status = buildLiteStatus({25.0, 1.25, 3.5, 0.02, 59.98});
        assert(status.networkText == "25.0 Mbps");
        assert(status.latencyText == "1.25 ms");
        assert(status.decodeText == "3.50 ms");
        assert(status.packetLossText == "0.02%");
        assert(status.fpsText == "59.98 FPS");
        assert(status.healthy);
    }

    {
        const auto status = buildLiteStatus({20.0, 12.0, 4.0, 0.0, 60.0});
        assert(!status.healthy);
    }

    {
        const auto status = buildLiteStatus({20.0, 2.0, 4.0, 1.5, 60.0});
        assert(!status.healthy);
    }

    assert(normalizeWifiSignal(-1) == 0.0f);
    assert(normalizeWifiSignal(0) == 0.0f);
    assert(normalizeWifiSignal(1) > 0.33f && normalizeWifiSignal(1) < 0.34f);
    assert(normalizeWifiSignal(2) > 0.66f && normalizeWifiSignal(2) < 0.67f);
    assert(normalizeWifiSignal(3) == 1.0f);
    assert(normalizeWifiSignal(10) == 1.0f);

    return 0;
}
