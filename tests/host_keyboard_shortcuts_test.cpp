#include "../app/src/features/input/HostKeyboardShortcuts.hpp"
#include "../app/src/utils/HostDeviceOs.hpp"

#include <cassert>
#include <optional>
#include <string>
#include <vector>

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

bool hasId(const std::vector<KeyboardShortcut>& presets, const std::string& id) {
    for (const auto& preset : presets) {
        if (preset.id == id)
            return true;
    }
    return false;
}

const KeyboardShortcut* findId(const std::vector<KeyboardShortcut>& presets,
                               const std::string& id) {
    for (const auto& preset : presets) {
        if (preset.id == id)
            return &preset;
    }
    return nullptr;
}

int main() {
    const auto left = moveActiveWindowShortcut(DisplayDirection::Left);
    verifyCommonSequence(left);
    assert(left[2].virtualKey == VirtualKeyLeftArrow);

    const auto right = moveActiveWindowShortcut(
        DisplayDirection::Right, HostDeviceOs::MacOS);
    verifyCommonSequence(right);
    assert(right[2].virtualKey == VirtualKeyRightArrow);

    const KeyboardShortcut valid{"custom", "My chord", {0x5B, 0x41}};
    assert(validateShortcut(valid).valid);
    assert(!validateShortcut({"bad", "", {0x41}}).valid);
    assert(!validateShortcut({"bad", "Duplicate", {0x41, 0x41}}).valid);
    assert(!validateShortcut({"bad", "Raw command", {0x1234}}).valid);

    const auto chord = shortcutEvents(valid.keys);
    assert(chord.size() == 4);
    assert(chord[0].virtualKey == 0x5B && chord[0].pressed);
    assert(chord[1].virtualKey == 0x41 && chord[1].pressed);
    assert(chord[2].virtualKey == 0x41 && !chord[2].pressed);
    assert(chord[3].virtualKey == 0x5B && !chord[3].pressed);

    assert(virtualKeyFromSymbol("win") == 0x5B);
    assert(virtualKeyFromSymbol("F24") == 0x87);
    assert(!virtualKeyFromSymbol("powershell.exe").has_value());
    assert(symbolFromVirtualKey(0x41) == std::optional<std::string>{"A"});

    const auto& windows = standardShortcuts(HostDeviceOs::Windows);
    assert(windows.size() >= 14);
    for (const auto& preset : windows)
        assert(validateShortcut(preset).valid);
    assert(hasId(windows, "game_bar"));
    assert(findId(windows, "game_bar")->windowsOnly);
    assert(findId(windows, "paste")->keys ==
           (std::vector<short>{VirtualKeyLeftControl, 0x56}));

    const auto& macos = standardShortcuts(HostDeviceOs::MacOS);
    assert(!hasId(macos, "game_bar"));
    assert(findId(macos, "paste")->keys ==
           (std::vector<short>{VirtualKeyLeftWindows, 0x56}));
    assert(findId(macos, "close_window")->keys ==
           (std::vector<short>{VirtualKeyLeftWindows, 0x51}));

    const auto& linux = standardShortcuts(HostDeviceOs::Linux);
    assert(!hasId(linux, "game_bar"));
    assert(findId(linux, "paste")->keys ==
           (std::vector<short>{VirtualKeyLeftControl, 0x56}));

    // Default overload stays Windows-compatible for callers.
    assert(standardShortcuts().size() == windows.size());
    return 0;
}
