#pragma once

#include <string_view>

namespace artemis::input {

enum class PointerMode {
    MultiTouch,
    Absolute,
    AbsoluteSwapped,
    TrackpadNatural,
    TrackpadGaming,
    Disabled,
};

struct NormalizedRegion {
    float left = 0.0f;
    float top = 0.0f;
    float right = 1.0f;
    float bottom = 1.0f;
};

struct PointerSettings {
    PointerMode mode = PointerMode::TrackpadNatural;
    bool localCursor = false;
    bool naturalScrolling = true;
    bool tapToClick = true;
    bool twoFingerRightClick = true;
    float sensitivityX = 1.0f;
    float sensitivityY = 1.0f;
    float scrollSensitivity = 1.0f;
    float dragThreshold = 8.0f;
    NormalizedRegion region;
};

PointerSettings validatePointerSettings(PointerSettings settings);
PointerMode pointerModeFromLegacyTouchscreen(bool touchscreenMouseMode);
const char* pointerModeName(PointerMode mode);
bool pointerModeFromName(std::string_view name, PointerMode& mode);
bool isTrackpadMode(PointerMode mode);
bool isAbsoluteMode(PointerMode mode);

} // namespace artemis::input
