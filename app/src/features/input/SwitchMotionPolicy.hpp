#pragma once

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
    // libnx exposes SevenSixAxisSensor / ConsoleSixAxisSensor on HOS 5.0+.
    bool libnxSevenSixAxisApiAvailable = false;

    // Keep this separate from API availability. Current libnx exposes the
    // SevenSixAxis state vector as undocumented fields, so Artemis must not
    // pretend those values are acceleration/gyro until their mapping is known.
    bool consoleMotionVectorsMapped = false;
};

SwitchMotionCapabilities detectSwitchMotionCapabilities();
bool canEnableConsoleMotionFallback(const SwitchMotionCapabilities& capabilities);

bool shouldForwardMotion(MotionSource source,
                         bool controllerReportsMotion,
                         const SwitchMotionOptions& options);

} // namespace artemis::input
