#include "SwitchRuntimeMetadata.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace artemis::benchmark {

#ifdef __SWITCH__
namespace {

bool readClockRate(PcvModuleId moduleId, uint32_t& outHz) {
    ClkrstSession session{};
    if (R_FAILED(clkrstOpenSession(&session, moduleId, 3)))
        return false;

    u32 hz = 0;
    const Result result = clkrstGetClockRate(&session, &hz);
    clkrstCloseSession(&session);

    if (R_FAILED(result))
        return false;

    outHz = hz;
    return true;
}

void collectClockRates(SwitchRuntimeMetadata& metadata) {
    if (hosversionAtLeast(8, 0, 0)) {
        if (R_FAILED(clkrstInitialize()))
            return;

        readClockRate(PcvModuleId_CpuBus, metadata.cpuClockHz);
        readClockRate(PcvModuleId_GPU, metadata.gpuClockHz);
        readClockRate(PcvModuleId_EMC, metadata.memoryClockHz);
        clkrstExit();
        return;
    }

    // Legacy firmware fallback. These APIs are read-only here and are only
    // used to annotate benchmark output, never to change Switch clocks.
    if (R_FAILED(pcvInitialize()))
        return;

    u32 hz = 0;
    if (R_SUCCEEDED(pcvGetClockRate(PcvModule_CpuBus, &hz)))
        metadata.cpuClockHz = hz;
    if (R_SUCCEEDED(pcvGetClockRate(PcvModule_GPU, &hz)))
        metadata.gpuClockHz = hz;
    if (R_SUCCEEDED(pcvGetClockRate(PcvModule_EMC, &hz)))
        metadata.memoryClockHz = hz;
    pcvExit();
}

} // namespace
#endif

SwitchRuntimeMetadata collectSwitchRuntimeMetadata() {
    SwitchRuntimeMetadata metadata;

#ifdef __SWITCH__
    metadata.platform = "Nintendo Switch";

    switch (appletGetOperationMode()) {
    case AppletOperationMode_Handheld:
        metadata.operationMode = "handheld";
        break;
    case AppletOperationMode_Console:
        metadata.operationMode = "docked";
        break;
    default:
        metadata.operationMode = "unknown";
        break;
    }

    if (R_SUCCEEDED(psmInitialize())) {
        u32 batteryPercent = 0;
        if (R_SUCCEEDED(psmGetBatteryChargePercentage(&batteryPercent)))
            metadata.batteryPercent = static_cast<int>(batteryPercent);

        bool chargingEnabled = false;
        if (R_SUCCEEDED(psmIsBatteryChargingEnabled(&chargingEnabled))) {
            metadata.batteryChargingKnown = true;
            metadata.batteryCharging = chargingEnabled;
        }
        psmExit();
    }

    collectClockRates(metadata);
#else
    metadata.platform = "non-switch";
#endif

    return metadata;
}

} // namespace artemis::benchmark
