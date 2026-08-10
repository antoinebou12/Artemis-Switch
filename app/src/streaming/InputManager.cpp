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
#include "../features/input/HostKeyboardShortcuts.hpp"
#include "../features/input/InputSettingsStore.hpp"
#include "../features/input/ControllerDiagnostics.hpp"
#include "../features/video/DisplayTransform.hpp"
#include "../features/video/DisplayTransformStore.hpp"
#include "../features/video/DisplayCoordinateMapper.hpp"

namespace {
artemis::input::MotionFallbackGate motionFallbackGate;

uint64_t steadyNowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
}

// Keep Moonlight-Switch's existing input implementation and intercept only the
// controller motion send boundary. This means mouse, keyboard, touch, rumble,
// button mapping, and controller state handling remain upstream-compatible.
static int ArtemisSendControllerMotionEvent(uint8_t controllerNumber,
                                            uint8_t motionType,
                                            float x, float y, float z) {
    const auto& options =
        artemis::input::SwitchMotionPolicyStore::instance().get();

    const bool handheldFallback = controllerNumber == UINT8_MAX;
    const auto source = handheldFallback
        ? artemis::input::MotionSource::Console
        : artemis::input::MotionSource::JoyCon;
    const uint64_t nowMs = steadyNowMs();
    const bool gyro = motionType == LI_MOTION_TYPE_GYRO;
    const int motionSlot = handheldFallback ? 0 : static_cast<int>(controllerNumber);
    artemis::input::ControllerDiagnostics::instance().recordMotion(
        motionSlot, gyro, x, y, z, handheldFallback, nowMs);

    if (!motionFallbackGate.shouldForward(source, nowMs, handheldFallback,
                                          options)) {
        return 0;
    }

    return LiSendControllerMotionEvent(
        handheldFallback ? 0 : controllerNumber, motionType, x, y, z);
}

// All includes used by InputManagerLegacy.inc are already loaded above, so this
// macro only rewrites the actual motion-send calls in the original source body.
#define LiSendControllerMotionEvent ArtemisSendControllerMotionEvent
#include "InputManagerLegacy.inc"
#undef LiSendControllerMotionEvent
