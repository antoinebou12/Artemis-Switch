#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace artemis::input {

enum class VirtualControlKind { Button, DPad, Trigger, Stick };
enum class VirtualBinding {
    A, B, X, Y, DPad, Start, Back, Guide, LeftBumper, RightBumper,
    LeftTrigger, RightTrigger, LeftStick, RightStick, LeftStickClick,
    RightStickClick
};

struct NormalizedControlRect {
    float x = 0;
    float y = 0;
    float width = 0.1f;
    float height = 0.1f;
};

struct TouchControl {
    std::string id;
    std::string label;
    VirtualControlKind kind = VirtualControlKind::Button;
    VirtualBinding binding = VirtualBinding::A;
    NormalizedControlRect rect;
    float opacity = 0.65f;
    int zOrder = 0;
};

struct TouchControlLayout {
    std::vector<TouchControl> controls;
};

struct TouchControlProfile {
    static constexpr int VERSION = 1;
    int version = VERSION;
    std::string id;
    std::string name;
    TouchControlLayout landscape;
    TouchControlLayout portrait;
};

struct VirtualGamepadState {
    uint16_t buttons = 0;
    uint8_t leftTrigger = 0;
    uint8_t rightTrigger = 0;
    int16_t leftStickX = 0;
    int16_t leftStickY = 0;
    int16_t rightStickX = 0;
    int16_t rightStickY = 0;
    bool leftStickTouched = false;
    bool rightStickTouched = false;
};

bool validateTouchControlProfile(TouchControlProfile& profile,
                                std::string* error = nullptr);
TouchControlProfile makeDefaultTouchControlProfile();
const TouchControl* hitTestTouchControl(const TouchControlLayout& layout,
                                        float x, float y);
VirtualGamepadState mergeVirtualGamepadInput(
    const VirtualGamepadState& physical,
    const VirtualGamepadState& virtualInput);
const char* virtualBindingName(VirtualBinding binding);
bool virtualBindingFromName(const std::string& name, VirtualBinding& binding);

} // namespace artemis::input
