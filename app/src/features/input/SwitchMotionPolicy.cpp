#include "SwitchMotionPolicy.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace artemis::input {

SwitchMotionCapabilities detectSwitchMotionCapabilities() {
    SwitchMotionCapabilities capabilities;

#ifdef __SWITCH__
    // SevenSixAxisSensor is available from HOS 5.0.0 onward in libnx.
    // We only advertise API availability here. HidSevenSixAxisSensorState
    // still exposes its payload as undocumented fields, so forwarding remains
    // disabled until Artemis has a verified acceleration/gyro mapping.
    capabilities.libnxSevenSixAxisApiAvailable = hosversionAtLeast(5, 0, 0);
#endif

    capabilities.consoleMotionVectorsMapped = false;
    return capabilities;
}

bool canEnableConsoleMotionFallback(
    const SwitchMotionCapabilities& capabilities) {
    return capabilities.libnxSevenSixAxisApiAvailable &&
           capabilities.consoleMotionVectorsMapped;
}

bool shouldForwardMotion(MotionSource source,
                         bool controllerReportsMotion,
                         const SwitchMotionOptions& options) {
    if (!options.allowGamepadMotionSensors)
        return false;

    if (source == MotionSource::Console)
        return options.allowConsoleMotionFallback && !controllerReportsMotion;

    return controllerReportsMotion;
}

} // namespace artemis::input
