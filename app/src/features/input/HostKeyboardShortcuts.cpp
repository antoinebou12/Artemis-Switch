#include "HostKeyboardShortcuts.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>

namespace artemis::input {

namespace {

const std::unordered_map<std::string, short>& keySymbols() {
    static const std::unordered_map<std::string, short> symbols = {
        {"BACKSPACE", 0x08}, {"TAB", 0x09}, {"ENTER", 0x0D},
        {"SHIFT", 0x10}, {"CTRL", 0x11}, {"ALT", 0x12},
        {"PAUSE", 0x13}, {"CAPSLOCK", 0x14}, {"ESC", 0x1B},
        {"SPACE", 0x20}, {"PAGEUP", 0x21}, {"PAGEDOWN", 0x22},
        {"END", 0x23}, {"HOME", 0x24}, {"LEFT", 0x25},
        {"UP", 0x26}, {"RIGHT", 0x27}, {"DOWN", 0x28},
        {"PRINTSCREEN", 0x2C}, {"INSERT", 0x2D}, {"DELETE", 0x2E},
        {"LWIN", 0x5B}, {"WIN", 0x5B}, {"RWIN", 0x5C}, {"CMD", 0x5B},
        {"SUPER", 0x5B}, {"META", 0x5B},
        {"NUMLOCK", 0x90}, {"SCROLLLOCK", 0x91},
        {"LSHIFT", 0xA0}, {"RSHIFT", 0xA1}, {"LCTRL", 0xA2},
        {"RCTRL", 0xA3}, {"LALT", 0xA4}, {"RALT", 0xA5},
        {"IME", 0x15}, {"OEM_1", 0xBA}, {"OEM_PLUS", 0xBB},
        {"OEM_COMMA", 0xBC}, {"OEM_MINUS", 0xBD},
        {"OEM_PERIOD", 0xBE}, {"OEM_2", 0xBF}, {"OEM_3", 0xC0},
        {"OEM_4", 0xDB}, {"OEM_5", 0xDC}, {"OEM_6", 0xDD},
        {"OEM_7", 0xDE},
    };
    return symbols;
}

std::string normalized(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const unsigned char c : value) {
        if (c == '-' || c == ' ')
            result.push_back('_');
        else
            result.push_back(static_cast<char>(std::toupper(c)));
    }
    return result;
}

std::vector<KeyboardShortcut> buildShortcuts(HostDeviceOs os) {
    std::vector<KeyboardShortcut> shortcuts = {
        {"escape", "Escape", {0x1B}},
        {"fullscreen", "Fullscreen", {0x7A}},
        {"toggle_fullscreen", "Toggle fullscreen", {0xA4, 0x0D}},
        {"reverse_tab", "Previous focus", {0xA0, 0x09}},
        {"move_window_left", "Move window left", {0x5B, 0xA0, 0x25}},
        {"move_window_right", "Move window right", {0x5B, 0xA0, 0x27}},
    };

    if (os == HostDeviceOs::MacOS) {
        shortcuts.push_back({"close_window", "Close window", {0x5B, 0x51}});
        shortcuts.push_back({"paste", "Paste", {0x5B, 0x56}});
        shortcuts.push_back({"meta", "Command menu", {0x5B}});
        shortcuts.push_back(
            {"task_manager", "Force quit", {0x5B, 0xA4, 0x1B}});
        shortcuts.push_back(
            {"task_switcher", "Task switcher", {0xA2, 0xA4, 0x09}});
        shortcuts.push_back(
            {"switch_host_ime", "Switch keyboard language", {0xA2, 0x20}});
        shortcuts.push_back({"desktop", "Hide application", {0x5B, 0x48}});
    } else {
        shortcuts.push_back({"close_window", "Close window", {0xA4, 0x73}});
        shortcuts.push_back({"paste", "Paste", {0xA2, 0x56}});
        shortcuts.push_back(
            {"meta", os == HostDeviceOs::Linux ? "Super key" : "Start menu",
             {0x5B}});
        shortcuts.push_back({"desktop", "Show desktop", {0x5B, 0x44}});
        shortcuts.push_back(
            {"task_switcher", "Task switcher", {0xA2, 0xA4, 0x09}});
        shortcuts.push_back(
            {"task_manager", "Task manager", {0xA2, 0xA0, 0x1B}});
        shortcuts.push_back(
            {"switch_host_ime", "Switch keyboard language", {0x5B, 0x20}});
        if (os == HostDeviceOs::Windows) {
            KeyboardShortcut gameBar{"game_bar", "Game Bar", {0x5B, 0x47}};
            gameBar.windowsOnly = true;
            shortcuts.push_back(std::move(gameBar));
        }
    }
    return shortcuts;
}

} // namespace

ShortcutValidation validateShortcut(const KeyboardShortcut& shortcut) {
    if (shortcut.name.empty())
        return {false, "Shortcut name is required"};
    if (shortcut.keys.empty())
        return {false, "A shortcut requires at least one key"};
    if (shortcut.keys.size() > MaxShortcutKeys)
        return {false, "A shortcut may contain at most eight keys"};

    std::unordered_set<short> seen;
    for (const short key : shortcut.keys) {
        if (key <= 0 || !symbolFromVirtualKey(key).has_value())
            return {false, "Shortcut contains an unknown virtual key"};
        if (!seen.insert(key).second)
            return {false, "Shortcut contains a duplicate key"};
    }
    return {true, {}};
}

std::vector<HostKeyEvent> shortcutEvents(const std::vector<short>& keys) {
    if (keys.empty() || keys.size() > MaxShortcutKeys)
        return {};
    std::vector<HostKeyEvent> events;
    events.reserve(keys.size() * 2);
    for (const short key : keys)
        events.push_back({key, true});
    for (auto it = keys.rbegin(); it != keys.rend(); ++it)
        events.push_back({*it, false});
    return events;
}

const std::vector<KeyboardShortcut>& standardShortcuts() {
    return standardShortcuts(HostDeviceOs::Windows);
}

const std::vector<KeyboardShortcut>& standardShortcuts(HostDeviceOs os) {
    static const std::vector<KeyboardShortcut> windows =
        buildShortcuts(HostDeviceOs::Windows);
    static const std::vector<KeyboardShortcut> macos =
        buildShortcuts(HostDeviceOs::MacOS);
    static const std::vector<KeyboardShortcut> linux =
        buildShortcuts(HostDeviceOs::Linux);
    switch (os) {
    case HostDeviceOs::MacOS:
        return macos;
    case HostDeviceOs::Linux:
        return linux;
    case HostDeviceOs::Windows:
    default:
        return windows;
    }
}

std::optional<short> virtualKeyFromSymbol(std::string_view symbol) {
    const std::string value = normalized(symbol);
    if (const auto it = keySymbols().find(value); it != keySymbols().end())
        return it->second;
    if (value.size() == 1 && value[0] >= 'A' && value[0] <= 'Z')
        return static_cast<short>(value[0]);
    if (value.size() == 1 && value[0] >= '0' && value[0] <= '9')
        return static_cast<short>(value[0]);
    if (value.size() >= 2 && value[0] == 'F') {
        try {
            const int n = std::stoi(value.substr(1));
            if (n >= 1 && n <= 24)
                return static_cast<short>(0x70 + (n - 1));
        } catch (...) {
        }
    }
    return std::nullopt;
}

std::optional<std::string> symbolFromVirtualKey(short virtualKey) {
    for (const auto& [symbol, key] : keySymbols()) {
        if (key == virtualKey)
            return symbol;
    }
    if (virtualKey >= 'A' && virtualKey <= 'Z')
        return std::string(1, static_cast<char>(virtualKey));
    if (virtualKey >= '0' && virtualKey <= '9')
        return std::string(1, static_cast<char>(virtualKey));
    if (virtualKey >= 0x70 && virtualKey <= 0x87)
        return "F" + std::to_string(virtualKey - 0x70 + 1);
    return std::nullopt;
}

std::array<HostKeyEvent, 6>
moveActiveWindowShortcut(DisplayDirection direction) {
    return moveActiveWindowShortcut(direction, HostDeviceOs::Windows);
}

std::array<HostKeyEvent, 6>
moveActiveWindowShortcut(DisplayDirection direction, HostDeviceOs os) {
    (void)os; // Same VK chord; host maps Meta appropriately.
    const short arrow = direction == DisplayDirection::Right
                            ? VirtualKeyRightArrow
                            : VirtualKeyLeftArrow;
    return {{
        {VirtualKeyLeftWindows, true},
        {VirtualKeyLeftShift, true},
        {arrow, true},
        {arrow, false},
        {VirtualKeyLeftShift, false},
        {VirtualKeyLeftWindows, false},
    }};
}

} // namespace artemis::input
