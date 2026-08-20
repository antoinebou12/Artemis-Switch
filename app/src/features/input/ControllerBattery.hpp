#pragma once

#include <cstdint>

namespace artemis::input {

// Mirrors the LI_BATTERY_STATE_* constants from Limelight.h. Duplicated here so
// the policy stays portable and unit-testable without the streaming stack.
constexpr uint8_t BatteryStateUnknown = 0x00;
constexpr uint8_t BatteryStateNotPresent = 0x01;
constexpr uint8_t BatteryStateDischarging = 0x02;
constexpr uint8_t BatteryStateCharging = 0x03;
constexpr uint8_t BatteryStateNotCharging = 0x04;
constexpr uint8_t BatteryStateFull = 0x05;
constexpr uint8_t BatteryPercentageUnknown = 0xFF;

// Battery reports are cheap but travel on the control stream, so they are only
// resent on a real change or after this idle interval.
constexpr uint64_t BatteryResendIntervalMs = 30000;

struct BatteryReading {
    uint8_t state = BatteryStateUnknown;
    uint8_t percentage = BatteryPercentageUnknown;

    [[nodiscard]] bool is_equal(const BatteryReading& other) const {
        return state == other.state && percentage == other.percentage;
    }
};

// libnx reports npad battery as a 0-4 level. `powered` means the pad is
// connected to a charger, `charging` means it is actually taking charge.
BatteryReading fromNpad(uint32_t batteryLevel, bool powered, bool charging,
                        bool connected);

// True when the reading should be pushed to the host. `lastSendMs` is 0 when
// nothing has been sent for the slot yet.
bool shouldResend(const BatteryReading& last, const BatteryReading& next,
                  uint64_t lastSendMs, uint64_t nowMs);

} // namespace artemis::input
