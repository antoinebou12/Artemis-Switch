#pragma once

#include <cstdint>
#include <string>

namespace artemis::benchmark {

struct SwitchRuntimeMetadata {
    std::string platform = "unknown";
    std::string operationMode = "unknown";

    int batteryPercent = -1;
    bool batteryChargingKnown = false;
    bool batteryCharging = false;

    // Clock reads are best-effort only. A value of 0 means the corresponding
    // clkrst service/session was unavailable to the current homebrew process.
    uint32_t cpuClockHz = 0;
    uint32_t gpuClockHz = 0;
    uint32_t memoryClockHz = 0;
};

SwitchRuntimeMetadata collectSwitchRuntimeMetadata();

} // namespace artemis::benchmark
