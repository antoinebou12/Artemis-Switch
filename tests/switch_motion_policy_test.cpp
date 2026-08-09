#include "../app/src/features/input/SwitchMotionPolicy.hpp"

#include <cassert>

using artemis::input::MotionSource;
using artemis::input::SwitchMotionCapabilities;
using artemis::input::SwitchMotionOptions;
using artemis::input::canEnableConsoleMotionFallback;
using artemis::input::detectSwitchMotionCapabilities;
using artemis::input::shouldForwardMotion;

int main() {
    SwitchMotionOptions options;
    assert(options.allowGamepadMotionSensors);
    assert(!options.allowConsoleMotionFallback);
    assert(shouldForwardMotion(MotionSource::JoyCon, true, options));
    assert(shouldForwardMotion(MotionSource::ProController, true, options));
    assert(!shouldForwardMotion(MotionSource::Console, false, options));

    SwitchMotionCapabilities apiOnly;
    apiOnly.libnxSevenSixAxisApiAvailable = true;
    apiOnly.consoleMotionVectorsMapped = false;
    assert(!canEnableConsoleMotionFallback(apiOnly));

    SwitchMotionCapabilities fullyMapped = apiOnly;
    fullyMapped.consoleMotionVectorsMapped = true;
    assert(canEnableConsoleMotionFallback(fullyMapped));

    options.allowConsoleMotionFallback = true;
    assert(shouldForwardMotion(MotionSource::Console, false, options));
    assert(!shouldForwardMotion(MotionSource::Console, true, options));

    options.allowGamepadMotionSensors = false;
    assert(!shouldForwardMotion(MotionSource::JoyCon, true, options));
    assert(!shouldForwardMotion(MotionSource::Console, false, options));

#ifndef __SWITCH__
    const auto runtime = detectSwitchMotionCapabilities();
    assert(!runtime.libnxSevenSixAxisApiAvailable);
    assert(!runtime.consoleMotionVectorsMapped);
#endif

    return 0;
}
