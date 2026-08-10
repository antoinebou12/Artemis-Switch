#ifdef __SWITCH__
#include <switch.h>
#endif

#include "InputManager.hpp"
#include "Limelight.h"
#include "Settings.hpp"
#include <borealis.hpp>
#include <streaming_view.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

#include "../features/input/SwitchMotionPolicy.hpp"
#include "../features/input/SwitchMotionPolicyStore.hpp"
#include "../features/input/ControllerTopology.hpp"

// Keep Moonlight-Switch's existing input implementation and intercept only the
// controller motion send boundary. This means mouse, keyboard, touch, rumble,
// button mapping, and controller state handling remain upstream-compatible.
static int ArtemisSendControllerMotionEvent(uint8_t controllerNumber,
                                            uint8_t motionType,
                                            float x, float y, float z) {
    const auto& options =
        artemis::input::SwitchMotionPolicyStore::instance().get();

    // Borealis currently reports controller IMU events through one callback,
    // without a separate console-IMU source identifier. Treat these as real
    // gamepad/Joy-Con motion and leave console fallback capability-gated until
    // a distinct source is available.
    if (!artemis::input::shouldForwardMotion(
            artemis::input::MotionSource::JoyCon,
            true,
            options)) {
        return 0;
    }

    return LiSendControllerMotionEvent(controllerNumber, motionType, x, y, z);
}

// All includes used by InputManagerLegacy.inc are already loaded above, so this
// macro only rewrites the actual motion-send calls in the original source body.
#define LiSendControllerMotionEvent ArtemisSendControllerMotionEvent
#include "InputManagerLegacy.inc"
#undef LiSendControllerMotionEvent
