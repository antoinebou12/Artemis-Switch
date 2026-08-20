#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <string>

namespace artemis::input {

struct DiagnosticVector3 {
    float x = 0;
    float y = 0;
    float z = 0;
};

struct ControllerDiagnosticSnapshot {
    static constexpr int VERSION = 1;

    int version = VERSION;
    int slot = 0;
    std::string identity = "Disconnected";
    bool connected = false;
    bool handheldMotionFallback = false;
    uint16_t buttons = 0;
    uint8_t leftTrigger = 0;
    uint8_t rightTrigger = 0;
    int16_t leftStickX = 0;
    int16_t leftStickY = 0;
    int16_t rightStickX = 0;
    int16_t rightStickY = 0;
    DiagnosticVector3 acceleration;
    DiagnosticVector3 gyroscope;
    float motionSampleRateHz = 0;
    uint64_t droppedMotionSamples = 0;
    uint16_t lastRumbleLow = 0;
    uint16_t lastRumbleHigh = 0;
    uint64_t lastRumbleAtMs = 0;
    uint8_t batteryState = 0;      // LI_BATTERY_STATE_UNKNOWN
    uint8_t batteryPercentage = 0xFF; // LI_BATTERY_PERCENTAGE_UNKNOWN
};

class ControllerDiagnostics {
public:
    static ControllerDiagnostics& instance();

    void recordController(int slot, bool connected, const std::string& identity,
                          uint16_t buttons, uint8_t leftTrigger,
                          uint8_t rightTrigger, int16_t leftStickX,
                          int16_t leftStickY, int16_t rightStickX,
                          int16_t rightStickY);
    void recordMotion(int slot, bool gyro, float x, float y, float z,
                      bool handheldFallback, uint64_t nowMs);
    void recordRumble(int slot, uint16_t low, uint16_t high, uint64_t nowMs);
    void recordBattery(int slot, uint8_t state, uint8_t percentage);
    void disconnectFrom(int firstSlot);
    [[nodiscard]] ControllerDiagnosticSnapshot snapshot(int slot) const;
    void reset();

private:
    struct SlotState {
        ControllerDiagnosticSnapshot snapshot;
        uint64_t lastMotionSampleMs = 0;
    };

    static constexpr int MAX_SLOTS = 4;
    mutable std::mutex m_mutex;
    std::array<SlotState, MAX_SLOTS> m_slots{};
};

} // namespace artemis::input
