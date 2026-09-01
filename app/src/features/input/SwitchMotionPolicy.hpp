#pragma once

#include <cstdint>

namespace artemis::input {

enum class MotionSource {
    JoyCon,
    ProController,
    Console,
};

// Which controller should provide the gyro/accel stream. Auto reproduces the
// stock selection order exactly (handheld first, then full-key, then left
// Joy-Con if connected, else right Joy-Con); the explicit choices override it.
enum class MotionSourcePreference {
    Auto,
    Handheld,
    JoyConLeft,
    JoyConRight,
};

// Which Borealis sensor slot to read next.
enum class MotionHandle {
    None,
    Handheld,
    FullKey,
    JoyLeft,
    JoyRight,
};

// Resolves the preference against the live controller topology. Returns the
// slot whose sixaxis input should be forwarded; Auto reproduces today's
// behaviour with no user-visible change.
MotionHandle selectMotionHandle(MotionSourcePreference preference,
                                bool handheldActive,
                                bool proConnected, bool leftConnected,
                                bool rightConnected);

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
