#include "../app/src/features/input/SwitchMotionPolicy.hpp"

#include <cassert>

using artemis::input::MotionSource;
using artemis::input::MotionFallbackGate;
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

    SwitchMotionCapabilities unsupported;
    assert(!canEnableConsoleMotionFallback(unsupported));

    SwitchMotionCapabilities documented;
    documented.documentedHandheldSixAxisAvailable = true;
    assert(canEnableConsoleMotionFallback(documented));

    options.allowConsoleMotionFallback = true;
    assert(shouldForwardMotion(MotionSource::Console, false, options));
    assert(!shouldForwardMotion(MotionSource::Console, true, options));

    options.allowGamepadMotionSensors = false;
    assert(!shouldForwardMotion(MotionSource::JoyCon, true, options));
    assert(!shouldForwardMotion(MotionSource::Console, false, options));

    MotionFallbackGate gate;
    options.allowGamepadMotionSensors = true;
    assert(!gate.shouldForward(MotionSource::Console, 1000, true, options));
    options.allowConsoleMotionFallback = true;
    assert(!gate.shouldForward(MotionSource::Console, 1000, true, options));
    assert(!gate.shouldForward(MotionSource::Console, 1499, true, options));
    assert(gate.shouldForward(MotionSource::Console, 1500, true, options));
    assert(!gate.shouldForward(MotionSource::Console, 1000, false, options));
    assert(gate.shouldForward(MotionSource::JoyCon, 1600, true, options));
    assert(!gate.shouldForward(MotionSource::Console, 2099, true, options));
    assert(gate.shouldForward(MotionSource::Console, 2100, true, options));
    assert(gate.shouldForward(MotionSource::ProController, 2200, false, options));
    assert(!gate.shouldForward(MotionSource::Console, 2800, false, options));
    gate.reset();
    assert(!gate.shouldForward(MotionSource::Console, 2800, true, options));
    assert(gate.shouldForward(MotionSource::Console, 3300, true, options));

#ifndef __SWITCH__
    const auto runtime = detectSwitchMotionCapabilities();
    assert(!runtime.documentedHandheldSixAxisAvailable);
#endif

    return 0;
}
