#include "../app/src/features/input/SwitchMotionPolicy.hpp"

#include <cassert>

using artemis::input::MotionSource;
using artemis::input::MotionFallbackGate;
using artemis::input::MotionHandle;
using artemis::input::MotionSourcePreference;
using artemis::input::SwitchMotionCapabilities;
using artemis::input::SwitchMotionOptions;
using artemis::input::canEnableConsoleMotionFallback;
using artemis::input::detectSwitchMotionCapabilities;
using artemis::input::selectMotionHandle;
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

    // Auto reproduces the stock selection order: handheld, then full-key,
    // then left Joy-Con if connected, else right.
    assert(selectMotionHandle(MotionSourcePreference::Auto,
                              /*handheld=*/true, true, true, true) ==
           MotionHandle::Handheld);
    assert(selectMotionHandle(MotionSourcePreference::Auto,
                              false, /*pro=*/true, true, true) ==
           MotionHandle::FullKey);
    assert(selectMotionHandle(MotionSourcePreference::Auto,
                              false, false, /*left=*/true, true) ==
           MotionHandle::JoyLeft);
    assert(selectMotionHandle(MotionSourcePreference::Auto,
                              false, false, false, /*right=*/true) ==
           MotionHandle::JoyRight);
    assert(selectMotionHandle(MotionSourcePreference::Auto,
                              false, false, false, false) ==
           MotionHandle::None);

    // Explicit preferences override the topology.
    assert(selectMotionHandle(MotionSourcePreference::Handheld,
                              false, true, true, true) == MotionHandle::None);
    assert(selectMotionHandle(MotionSourcePreference::JoyConRight,
                              true, true, true, true) ==
           MotionHandle::JoyRight);
    assert(selectMotionHandle(MotionSourcePreference::JoyConLeft,
                              true, true, /*left=*/false, true) ==
           MotionHandle::None);
    assert(selectMotionHandle(MotionSourcePreference::JoyConLeft,
                              true, true, /*left=*/true, false) ==
           MotionHandle::JoyLeft);

    return 0;
}
