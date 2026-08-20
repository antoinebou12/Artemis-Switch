#include "ControllerBattery.hpp"

#include <cassert>

using namespace artemis::input;

int main() {
    // --- Level to percentage ---------------------------------------------
    assert(fromNpad(0, false, false, true).percentage == 0);
    assert(fromNpad(1, false, false, true).percentage == 25);
    assert(fromNpad(2, false, false, true).percentage == 50);
    assert(fromNpad(3, false, false, true).percentage == 75);
    assert(fromNpad(4, false, false, true).percentage == 100);
    // Out-of-range levels clamp instead of wrapping past 100%.
    assert(fromNpad(9, false, false, true).percentage == 100);

    // --- States ------------------------------------------------------------
    assert(fromNpad(2, false, false, true).state == BatteryStateDischarging);
    assert(fromNpad(2, true, true, true).state == BatteryStateCharging);
    assert(fromNpad(2, true, false, true).state == BatteryStateNotCharging);
    assert(fromNpad(4, true, false, true).state == BatteryStateFull);
    // Charging wins over a full level: the pad is still taking charge.
    assert(fromNpad(4, true, true, true).state == BatteryStateCharging);

    const auto absent = fromNpad(3, true, true, false);
    assert(absent.state == BatteryStateNotPresent);
    assert(absent.percentage == BatteryPercentageUnknown);

    // --- Resend throttle ---------------------------------------------------
    const BatteryReading none;
    const auto half = fromNpad(2, false, false, true);
    const auto halfCharging = fromNpad(2, true, true, true);
    const auto threeQuarters = fromNpad(3, false, false, true);

    // First send for a slot always goes out.
    assert(shouldResend(none, half, 0, 1000));
    // Identical reading inside the window is suppressed.
    assert(!shouldResend(half, half, 1000, 1000 + BatteryResendIntervalMs - 1));
    // ... and released once the window elapses.
    assert(shouldResend(half, half, 1000, 1000 + BatteryResendIntervalMs));
    // A state change goes out immediately.
    assert(shouldResend(half, halfCharging, 1000, 1001));
    // So does a percentage change.
    assert(shouldResend(half, threeQuarters, 1000, 1001));
    // Unknown readings are never sent.
    assert(!shouldResend(half, none, 1000, 1000 + BatteryResendIntervalMs * 4));
    // A backwards clock must not spam the control stream.
    assert(!shouldResend(half, half, 5000, 1000));

    return 0;
}
