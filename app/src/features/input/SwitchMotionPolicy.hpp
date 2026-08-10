#pragma once

#include <cstdint>

namespace artemis::input {

enum class MotionSource {
    JoyCon,
    ProController,
    Console,
};

struct SwitchMotionOptions {
    bool allowGamepadMotionSensors = true;
    // Console motion is intentionally opt-in and remains disabled unless the
    // runtime reports a fully usable console-motion implementation.
    bool allowConsoleMotionFallback = false;
};

struct SwitchMotionCapabilities {
    // Borealis exposes the documented libnx HidSixAxisSensorState for the
    // handheld Npad sensor handle. No SevenSixAxis payload is used.
    bool documentedHandheldSixAxisAvailable = false;
};

SwitchMotionCapabilities detectSwitchMotionCapabilities();
bool canEnableConsoleMotionFallback(const SwitchMotionCapabilities& capabilities);

bool shouldForwardMotion(MotionSource source,
                         bool controllerReportsMotion,
                         const SwitchMotionOptions& options);

class MotionFallbackGate {
public:
    static constexpr uint64_t FALLBACK_DELAY_MS = 500;

    bool shouldForward(MotionSource source, uint64_t nowMs, bool handheld,
                       const SwitchMotionOptions& options);
    void reset();
    [[nodiscard]] bool controllerRecentlyActive(uint64_t nowMs) const;

private:
    uint64_t m_lastControllerSampleMs = 0;
    uint64_t m_observationStartMs = 0;
    bool m_hasControllerSample = false;
    bool m_hasObservationStart = false;
};

} // namespace artemis::input
