#include "SwitchMotionPolicy.hpp"

namespace artemis::input {

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
