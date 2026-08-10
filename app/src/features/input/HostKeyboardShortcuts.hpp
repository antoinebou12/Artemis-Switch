#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

constexpr std::size_t MaxShortcutKeys = 8;

struct KeyboardShortcut {
    std::string id;
    std::string name;
    std::vector<short> keys;
    bool enabled = true;
};

struct ShortcutValidation {
    bool valid = false;
    std::string error;
};

ShortcutValidation validateShortcut(const KeyboardShortcut& shortcut);
std::vector<HostKeyEvent> shortcutEvents(const std::vector<short>& keys);
const std::vector<KeyboardShortcut>& standardShortcuts();
std::optional<short> virtualKeyFromSymbol(std::string_view symbol);
std::optional<std::string> symbolFromVirtualKey(short virtualKey);

std::array<HostKeyEvent, 6>
moveActiveWindowShortcut(DisplayDirection direction);

} // namespace artemis::input
