#include "ControllerDiagnostics.hpp"

#include <algorithm>
#include <cmath>

namespace artemis::input {
namespace {
constexpr float EXPECTED_SAMPLE_PERIOD_MS = 1000.0f / 60.0f;
}

ControllerDiagnostics& ControllerDiagnostics::instance() {
    static ControllerDiagnostics diagnostics;
    return diagnostics;
}

void ControllerDiagnostics::recordController(
    int slot, bool connected, const std::string& identity, uint16_t buttons,
    uint8_t leftTrigger, uint8_t rightTrigger, int16_t leftStickX,
    int16_t leftStickY, int16_t rightStickX, int16_t rightStickY) {
    if (slot < 0 || slot >= MAX_SLOTS)
        return;
    std::scoped_lock lock(m_mutex);
    auto& value = m_slots[slot].snapshot;
    value.slot = slot;
    value.connected = connected;
    value.identity = connected ? identity : "Disconnected";
    value.buttons = connected ? buttons : 0;
    value.leftTrigger = connected ? leftTrigger : 0;
    value.rightTrigger = connected ? rightTrigger : 0;
    value.leftStickX = connected ? leftStickX : 0;
    value.leftStickY = connected ? leftStickY : 0;
    value.rightStickX = connected ? rightStickX : 0;
    value.rightStickY = connected ? rightStickY : 0;
}

void ControllerDiagnostics::recordMotion(int slot, bool gyro, float x, float y,
                                         float z, bool handheldFallback,
                                         uint64_t nowMs) {
    if (slot < 0 || slot >= MAX_SLOTS)
        return;
    std::scoped_lock lock(m_mutex);
    auto& state = m_slots[slot];
    auto& value = state.snapshot;
    value.slot = slot;
    value.handheldMotionFallback = handheldFallback;
    const DiagnosticVector3 vector{x, y, z};
    if (gyro) {
        value.gyroscope = vector;
        if (state.lastMotionSampleMs != 0 && nowMs > state.lastMotionSampleMs) {
            const uint64_t delta = nowMs - state.lastMotionSampleMs;
            const float currentRate = 1000.0f / static_cast<float>(delta);
            value.motionSampleRateHz = value.motionSampleRateHz == 0
                ? currentRate
                : value.motionSampleRateHz * 0.85f + currentRate * 0.15f;
            const auto expected = static_cast<uint64_t>(std::llround(
                static_cast<float>(delta) / EXPECTED_SAMPLE_PERIOD_MS));
            if (expected > 1)
                value.droppedMotionSamples += expected - 1;
        }
        state.lastMotionSampleMs = nowMs;
    } else {
        value.acceleration = vector;
    }
}

void ControllerDiagnostics::recordRumble(int slot, uint16_t low, uint16_t high,
                                         uint64_t nowMs) {
    if (slot < 0 || slot >= MAX_SLOTS)
        return;
    std::scoped_lock lock(m_mutex);
    auto& value = m_slots[slot].snapshot;
    value.slot = slot;
    value.lastRumbleLow = low;
    value.lastRumbleHigh = high;
    value.lastRumbleAtMs = nowMs;
}

void ControllerDiagnostics::disconnectFrom(int firstSlot) {
    std::scoped_lock lock(m_mutex);
    for (int slot = std::clamp(firstSlot, 0, MAX_SLOTS); slot < MAX_SLOTS;
         ++slot) {
        auto& value = m_slots[slot].snapshot;
        value.connected = false;
        value.identity = "Disconnected";
        value.buttons = 0;
        value.leftTrigger = value.rightTrigger = 0;
        value.leftStickX = value.leftStickY = 0;
        value.rightStickX = value.rightStickY = 0;
    }
}

ControllerDiagnosticSnapshot ControllerDiagnostics::snapshot(int slot) const {
    std::scoped_lock lock(m_mutex);
    if (slot < 0 || slot >= MAX_SLOTS)
        return {};
    return m_slots[slot].snapshot;
}

void ControllerDiagnostics::reset() {
    std::scoped_lock lock(m_mutex);
    m_slots = {};
}

} // namespace artemis::input
