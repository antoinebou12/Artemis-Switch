#pragma once

#include <array>

namespace artemis::input {

enum class DisplayDirection { Left, Right };

struct HostKeyEvent {
    short virtualKey = 0;
    bool pressed = false;
};

constexpr short VirtualKeyLeftWindows = 0x5B;
constexpr short VirtualKeyLeftShift = 0xA0;
constexpr short VirtualKeyLeftArrow = 0x25;
constexpr short VirtualKeyRightArrow = 0x27;

std::array<HostKeyEvent, 6>
moveActiveWindowShortcut(DisplayDirection direction);

} // namespace artemis::input
