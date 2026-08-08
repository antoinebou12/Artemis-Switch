#pragma once

namespace artemis::input {

enum class MotionSource {
    JoyCon,
    ProController,
    Console,
};

struct SwitchMotionOptions {
    bool allowGamepadMotionSensors = true;
    bool allowConsoleMotionFallback = false;
};

bool shouldForwardMotion(MotionSource source,
                         bool controllerReportsMotion,
                         const SwitchMotionOptions& options);

} // namespace artemis::input
