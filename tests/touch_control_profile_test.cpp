#include "../app/src/features/input/TouchControlProfile.hpp"

#include <cassert>

using namespace artemis::input;

int main() {
    auto profile = makeDefaultTouchControlProfile();
    std::string error;
    assert(validateTouchControlProfile(profile, &error));
    assert(profile.landscape.controls.size() == 16);
    assert(profile.portrait.controls.size() == 16);
    for (int i = 0; i <= static_cast<int>(VirtualBinding::RightStickClick); ++i) {
        VirtualBinding parsed{};
        const auto binding = static_cast<VirtualBinding>(i);
        assert(virtualBindingFromName(virtualBindingName(binding), parsed));
        assert(parsed == binding);
    }
    assert(!virtualBindingFromName("exec", profile.landscape.controls[0].binding));

    auto* top = hitTestTouchControl(profile.landscape, .90f, .70f);
    assert(top && top->binding == VirtualBinding::A);
    assert(!hitTestTouchControl(profile.landscape, .5f, .5f));

    VirtualGamepadState physical;
    physical.buttons = 0x1;
    physical.leftTrigger = 100;
    physical.leftStickX = 123;
    VirtualGamepadState touch;
    touch.buttons = 0x2;
    touch.leftTrigger = 200;
    touch.leftStickTouched = true;
    touch.leftStickX = -300;
    touch.leftStickY = 400;
    const auto merged = mergeVirtualGamepadInput(physical, touch);
    assert(merged.buttons == 0x3);
    assert(merged.leftTrigger == 200);
    assert(merged.leftStickX == -300 && merged.leftStickY == 400);
    assert(merged.rightStickX == physical.rightStickX);

    profile.landscape.controls[0].rect.x = -1;
    assert(!validateTouchControlProfile(profile, &error));
    return 0;
}
