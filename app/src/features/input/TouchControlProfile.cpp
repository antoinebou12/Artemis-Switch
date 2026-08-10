#include "TouchControlProfile.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

namespace artemis::input {
namespace {
bool finiteRect(const NormalizedControlRect& rect) {
    return std::isfinite(rect.x) && std::isfinite(rect.y) &&
        std::isfinite(rect.width) && std::isfinite(rect.height);
}

TouchControl control(std::string id, std::string label,
                     VirtualControlKind kind, VirtualBinding binding,
                     float x, float y, float width, float height, int z) {
    return {std::move(id), std::move(label), kind, binding,
            {x, y, width, height}, 0.65f, z};
}

TouchControlLayout portraitFromLandscape(const TouchControlLayout& source) {
    TouchControlLayout result = source;
    for (auto& item : result.controls) {
        const auto rect = item.rect;
        item.rect.x = rect.y;
        item.rect.y = 1.0f - rect.x - rect.width;
        item.rect.width = rect.height;
        item.rect.height = rect.width;
    }
    return result;
}
}

bool validateTouchControlProfile(TouchControlProfile& profile,
                                std::string* error) {
    const auto fail = [error](const std::string& message) {
        if (error) *error = message;
        return false;
    };
    if (profile.version != TouchControlProfile::VERSION)
        return fail("Unsupported touch-control profile version");
    if (profile.id.empty() || profile.id.size() > 64 ||
        profile.name.empty() || profile.name.size() > 96)
        return fail("Profile ID or name is invalid");

    auto validateLayout = [&fail](TouchControlLayout& layout) {
        if (layout.controls.size() > 64)
            return fail("A layout may contain at most 64 controls");
        std::set<std::string> ids;
        for (auto& item : layout.controls) {
            if (item.id.empty() || item.id.size() > 64 ||
                item.label.size() > 24 || !ids.insert(item.id).second)
                return fail("Control IDs must be unique and non-empty");
            if (!finiteRect(item.rect) || item.rect.width < 0.035f ||
                item.rect.height < 0.035f || item.rect.width > 0.5f ||
                item.rect.height > 0.5f || item.rect.x < 0 || item.rect.y < 0 ||
                item.rect.x + item.rect.width > 1.0f ||
                item.rect.y + item.rect.height > 1.0f)
                return fail("Control rectangle is outside the normalized layout");
            item.opacity = std::clamp(item.opacity, 0.15f, 1.0f);
            item.zOrder = std::clamp(item.zOrder, -1000, 1000);
        }
        return true;
    };
    if (!validateLayout(profile.landscape) ||
        !validateLayout(profile.portrait))
        return false;
    if (error) error->clear();
    return true;
}

TouchControlProfile makeDefaultTouchControlProfile() {
    TouchControlProfile profile;
    profile.id = "artemis-default";
    profile.name = "Artemis Default";
    auto& controls = profile.landscape.controls;
    int z = 0;
    controls.push_back(control("dpad", "D", VirtualControlKind::DPad,
        VirtualBinding::DPad, .04f, .58f, .19f, .34f, z++));
    controls.push_back(control("left-stick", "LS", VirtualControlKind::Stick,
        VirtualBinding::LeftStick, .25f, .68f, .15f, .27f, z++));
    controls.push_back(control("right-stick", "RS", VirtualControlKind::Stick,
        VirtualBinding::RightStick, .62f, .68f, .15f, .27f, z++));
    controls.push_back(control("a", "A", VirtualControlKind::Button,
        VirtualBinding::A, .88f, .67f, .075f, .13f, z++));
    controls.push_back(control("b", "B", VirtualControlKind::Button,
        VirtualBinding::B, .80f, .78f, .075f, .13f, z++));
    controls.push_back(control("x", "X", VirtualControlKind::Button,
        VirtualBinding::X, .80f, .56f, .075f, .13f, z++));
    controls.push_back(control("y", "Y", VirtualControlKind::Button,
        VirtualBinding::Y, .72f, .67f, .075f, .13f, z++));
    controls.push_back(control("lb", "LB", VirtualControlKind::Button,
        VirtualBinding::LeftBumper, .04f, .05f, .12f, .09f, z++));
    controls.push_back(control("rb", "RB", VirtualControlKind::Button,
        VirtualBinding::RightBumper, .84f, .05f, .12f, .09f, z++));
    controls.push_back(control("lt", "LT", VirtualControlKind::Trigger,
        VirtualBinding::LeftTrigger, .18f, .05f, .12f, .09f, z++));
    controls.push_back(control("rt", "RT", VirtualControlKind::Trigger,
        VirtualBinding::RightTrigger, .70f, .05f, .12f, .09f, z++));
    controls.push_back(control("back", "Back", VirtualControlKind::Button,
        VirtualBinding::Back, .40f, .08f, .07f, .07f, z++));
    controls.push_back(control("guide", "Guide", VirtualControlKind::Button,
        VirtualBinding::Guide, .465f, .05f, .07f, .07f, z++));
    controls.push_back(control("start", "Start", VirtualControlKind::Button,
        VirtualBinding::Start, .53f, .08f, .07f, .07f, z++));
    controls.push_back(control("l3", "L3", VirtualControlKind::Button,
        VirtualBinding::LeftStickClick, .29f, .58f, .07f, .09f, z++));
    controls.push_back(control("r3", "R3", VirtualControlKind::Button,
        VirtualBinding::RightStickClick, .66f, .58f, .07f, .09f, z++));
    profile.portrait = portraitFromLandscape(profile.landscape);
    validateTouchControlProfile(profile);
    return profile;
}

const TouchControl* hitTestTouchControl(const TouchControlLayout& layout,
                                        float x, float y) {
    const TouchControl* result = nullptr;
    for (const auto& item : layout.controls) {
        if (x >= item.rect.x && x <= item.rect.x + item.rect.width &&
            y >= item.rect.y && y <= item.rect.y + item.rect.height &&
            (!result || item.zOrder >= result->zOrder))
            result = &item;
    }
    return result;
}

VirtualGamepadState mergeVirtualGamepadInput(
    const VirtualGamepadState& physical,
    const VirtualGamepadState& virtualInput) {
    VirtualGamepadState result = physical;
    result.buttons |= virtualInput.buttons;
    result.leftTrigger = std::max(result.leftTrigger, virtualInput.leftTrigger);
    result.rightTrigger = std::max(result.rightTrigger, virtualInput.rightTrigger);
    if (virtualInput.leftStickTouched) {
        result.leftStickX = virtualInput.leftStickX;
        result.leftStickY = virtualInput.leftStickY;
        result.leftStickTouched = true;
    }
    if (virtualInput.rightStickTouched) {
        result.rightStickX = virtualInput.rightStickX;
        result.rightStickY = virtualInput.rightStickY;
        result.rightStickTouched = true;
    }
    return result;
}

const char* virtualBindingName(VirtualBinding binding) {
    static constexpr std::array names = {
        "a", "b", "x", "y", "dpad", "start", "back", "guide", "lb",
        "rb", "lt", "rt", "left-stick", "right-stick", "l3", "r3"};
    return names[static_cast<size_t>(binding)];
}

bool virtualBindingFromName(const std::string& name, VirtualBinding& binding) {
    for (int i = 0; i <= static_cast<int>(VirtualBinding::RightStickClick); ++i) {
        const auto candidate = static_cast<VirtualBinding>(i);
        if (name == virtualBindingName(candidate)) {
            binding = candidate;
            return true;
        }
    }
    return false;
}

} // namespace artemis::input
