#include "../app/src/features/input/ControllerDiagnostics.hpp"

#include <cassert>

using artemis::input::ControllerDiagnostics;

int main() {
    auto& diagnostics = ControllerDiagnostics::instance();
    diagnostics.reset();

    diagnostics.recordController(0, true, "Handheld", 0x1234, 20, 30,
                                 -100, 200, -300, 400);
    auto value = diagnostics.snapshot(0);
    assert(value.version == 1);
    assert(value.connected);
    assert(value.identity == "Handheld");
    assert(value.buttons == 0x1234);
    assert(value.rightStickY == 400);

    diagnostics.recordMotion(0, false, 1, 2, 3, true, 1000);
    diagnostics.recordMotion(0, true, 4, 5, 6, true, 1000);
    diagnostics.recordMotion(0, true, 7, 8, 9, true, 1050);
    value = diagnostics.snapshot(0);
    assert(value.handheldMotionFallback);
    assert(value.acceleration.y == 2);
    assert(value.gyroscope.z == 9);
    assert(value.motionSampleRateHz > 0);
    assert(value.droppedMotionSamples == 2);

    diagnostics.recordRumble(0, 100, 200, 1100);
    value = diagnostics.snapshot(0);
    assert(value.lastRumbleLow == 100);
    assert(value.lastRumbleHigh == 200);
    assert(value.lastRumbleAtMs == 1100);

    diagnostics.disconnectFrom(0);
    value = diagnostics.snapshot(0);
    assert(!value.connected);
    assert(value.buttons == 0);
    return 0;
}
