#include "../app/src/features/input/SwitchMotionPolicy.hpp"

#include <cassert>

using artemis::input::MotionSource;
using artemis::input::SwitchMotionOptions;
using artemis::input::shouldForwardMotion;

int main() {
    SwitchMotionOptions options;
    assert(shouldForwardMotion(MotionSource::JoyCon, true, options));
    assert(shouldForwardMotion(MotionSource::ProController, true, options));
    assert(!shouldForwardMotion(MotionSource::Console, false, options));

    options.allowConsoleMotionFallback = true;
    assert(shouldForwardMotion(MotionSource::Console, false, options));
    assert(!shouldForwardMotion(MotionSource::Console, true, options));

    options.allowGamepadMotionSensors = false;
    assert(!shouldForwardMotion(MotionSource::JoyCon, true, options));
    assert(!shouldForwardMotion(MotionSource::Console, false, options));
    return 0;
}
