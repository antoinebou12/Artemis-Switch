#pragma once

#include "HostDeviceOs.hpp"

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
constexpr short VirtualKeyLeftControl = 0xA2;
constexpr short VirtualKeyLeftAlt = 0xA4;

constexpr std::size_t MaxShortcutKeys = 8;

struct KeyboardShortcut {
    std::string id;
    std::string name; // English fallback / debug; UI uses i18n by id
    std::vector<short> keys;
    bool enabled = true;
    bool windowsOnly = false;
};

struct ShortcutValidation {
    bool valid = false;
    std::string error;
};

ShortcutValidation validateShortcut(const KeyboardShortcut& shortcut);
std::vector<HostKeyEvent> shortcutEvents(const std::vector<short>& keys);
const std::vector<KeyboardShortcut>& standardShortcuts();
const std::vector<KeyboardShortcut>& standardShortcuts(HostDeviceOs os);
std::optional<short> virtualKeyFromSymbol(std::string_view symbol);
std::optional<std::string> symbolFromVirtualKey(short virtualKey);

std::array<HostKeyEvent, 6>
moveActiveWindowShortcut(DisplayDirection direction);
std::array<HostKeyEvent, 6>
moveActiveWindowShortcut(DisplayDirection direction, HostDeviceOs os);

} // namespace artemis::input
