#include "ControllerBattery.hpp"

namespace artemis::input {

BatteryReading fromNpad(uint32_t batteryLevel, bool powered, bool charging,
                        bool connected) {
    BatteryReading reading;
    if (!connected) {
        reading.state = BatteryStateNotPresent;
        reading.percentage = BatteryPercentageUnknown;
        return reading;
    }

    // libnx exposes levels 0-4; anything above 4 is treated as full rather than
    // wrapping past 100%.
    const uint32_t clamped = batteryLevel > 4 ? 4 : batteryLevel;
    reading.percentage = static_cast<uint8_t>(clamped * 25);

    if (charging)
        reading.state = BatteryStateCharging;
    else if (powered)
        reading.state = clamped >= 4 ? BatteryStateFull : BatteryStateNotCharging;
    else
        reading.state = BatteryStateDischarging;

    return reading;
}

bool shouldResend(const BatteryReading& last, const BatteryReading& next,
                  uint64_t lastSendMs, uint64_t nowMs) {
    if (next.state == BatteryStateUnknown)
        return false;
    if (lastSendMs == 0)
        return true;
    if (!last.is_equal(next))
        return true;
    // Guard against a clock that went backwards; treat it as "not yet due".
    if (nowMs < lastSendMs)
        return false;
    return nowMs - lastSendMs >= BatteryResendIntervalMs;
}

} // namespace artemis::input
