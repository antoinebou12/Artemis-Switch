#include "../app/src/features/input/HostKeyboardShortcuts.hpp"

#include <cassert>

using namespace artemis::input;

void verifyCommonSequence(const std::array<HostKeyEvent, 6>& events) {
    assert(events[0].virtualKey == VirtualKeyLeftWindows && events[0].pressed);
    assert(events[1].virtualKey == VirtualKeyLeftShift && events[1].pressed);
    assert(events[2].pressed);
    assert(!events[3].pressed);
    assert(events[3].virtualKey == events[2].virtualKey);
    assert(events[4].virtualKey == VirtualKeyLeftShift && !events[4].pressed);
    assert(events[5].virtualKey == VirtualKeyLeftWindows && !events[5].pressed);
}

int main() {
    const auto left = moveActiveWindowShortcut(DisplayDirection::Left);
    verifyCommonSequence(left);
    assert(left[2].virtualKey == VirtualKeyLeftArrow);

    const auto right = moveActiveWindowShortcut(DisplayDirection::Right);
    verifyCommonSequence(right);
    assert(right[2].virtualKey == VirtualKeyRightArrow);
    return 0;
}
