#include "HostKeyboardShortcuts.hpp"

namespace artemis::input {

std::array<HostKeyEvent, 6>
moveActiveWindowShortcut(DisplayDirection direction) {
    const short arrow = direction == DisplayDirection::Right
                            ? VirtualKeyRightArrow
                            : VirtualKeyLeftArrow;
    return {{{VirtualKeyLeftWindows, true},
             {VirtualKeyLeftShift, true},
             {arrow, true},
             {arrow, false},
             {VirtualKeyLeftShift, false},
             {VirtualKeyLeftWindows, false}}};
}

} // namespace artemis::input
