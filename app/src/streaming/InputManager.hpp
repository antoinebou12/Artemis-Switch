//
//  InputManager.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 27.05.2021.
//

#pragma once

#include "Singleton.hpp"
#include "keyboard_view.hpp"
#include "../features/input/ControllerBattery.hpp"
#include "../features/input/StickDeadzone.hpp"
#include "../features/input/ControllerSessionReset.hpp"
#include <borealis.hpp>
#include <chrono>
#include <optional>
#include <vector>

// Moonlight ready gamepad
struct GamepadState {
    short buttonFlags = 0;
    unsigned char leftTrigger = 0;
    unsigned char rightTrigger = 0;
    short leftStickX = 0;
    short leftStickY = 0;
    short rightStickX = 0;
    short rightStickY = 0;

    bool is_equal(GamepadState other) {
        return buttonFlags == other.buttonFlags &&
               leftTrigger == other.leftTrigger &&
               rightTrigger == other.rightTrigger &&
               leftStickX == other.leftStickX &&
               leftStickY == other.leftStickY &&
               rightStickX == other.rightStickX &&
               rightStickY == other.rightStickY;
    }
};

struct MouseStateS {
    float scroll_y = 0;
    bool l_pressed = 0;
    bool m_pressed = 0;
    bool r_pressed = 0;
};

struct RumbleValues {
    unsigned short lowFreqMotor;
    unsigned short highFreqMotor;
    uint16_t leftTriggerMotor;
    uint16_t rightTriggerMotor;
};

class MoonlightInputManager : public Singleton<MoonlightInputManager> {
  public:
    MoonlightInputManager();
    void dropInput();
    // Forgets everything the previous stream told the host, so a new session
    // re-announces its pads instead of assuming the host still knows them.
    void resetForNewSession();
    // Stops every motor. Safe to call when no stream is running.
    void stopAllRumble();
    void handleInput(bool ignoreTouch = false);
    // Sample pads into ControllerDiagnostics without requiring stream focus.
    void sampleDiagnostics();
    void handleRumble(unsigned short controller, unsigned short lowFreqMotor, unsigned short highFreqMotor);
    void handleRumbleTriggers(unsigned short controller, unsigned short lowFreqMotor, unsigned short highFreqMotor);
    void updateTouchScreenPanDelta(brls::PanGestureStatus panStatus);
    void reloadButtonMappingLayout();
    void setInputEnabled(bool enabled) { inputEnabled = enabled; }
    static void leftMouseClick();
    static void rightMouseClick();
    static bool sendKeyboardShortcut(const std::vector<short>& keys);
    static bool moveActiveWindowToDisplay(bool moveRight);

  private:
    enum class DesktopScrollAxis {
        None,
        Horizontal,
        Vertical,
    };

    RumbleValues rumbleCache[GAMEPADS_MAX];
    GamepadState lastGamepadStates[GAMEPADS_MAX];
    artemis::input::BatteryReading lastBatteryReadings[GAMEPADS_MAX];
    uint64_t lastBatterySendMs[GAMEPADS_MAX] = {};
    brls::ControllerButton mappingButtons[brls::_BUTTON_MAX];
    std::optional<brls::PanGestureStatus> panStatus;
    std::map<uint32_t, bool> activeTouchIDs;
    brls::Point desktopMouseRemainder = {0, 0};
    brls::Point desktopScrollRemainder = {0, 0};
    float pendingHorizontalScroll = 0;
    unsigned pendingHorizontalScrollEvents = 0;
    DesktopScrollAxis desktopScrollAxis = DesktopScrollAxis::None;
    std::chrono::steady_clock::time_point lastDesktopScrollEvent;
    bool inputDropped = false;
    bool inputEnabled = true;
    int lastControllerCount = 0;
    // Mouse and scroll pacing state. These were function-level statics, which
    // meant they outlived the stream that produced them and leaked into the
    // next session.
    MouseStateS lastMouseState = {};
    std::chrono::high_resolution_clock::time_point lastScrollSentAt =
        std::chrono::high_resolution_clock::now();

    brls::ControllerState mapController(brls::ControllerState controller);
    static short glfwKeyToVKKey(brls::BrlsKeyboardScancode key);
    void sendRelativeMouseMove(brls::Point offset);
    void handleControllerBattery(int slot, uint64_t nowMs);
    void handleDesktopMouseScroll(brls::Point scroll);
    void sendDesktopMouseScroll(brls::Point scroll);

    GamepadState getControllerState(int controllerNum, bool specialKey);
    void handleControllers(bool specialKey);
};
