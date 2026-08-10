#include "PointerSettings.hpp"

#include <algorithm>

namespace artemis::input {

PointerSettings validatePointerSettings(PointerSettings settings) {
    settings.sensitivityX = std::clamp(settings.sensitivityX, 0.1f, 5.0f);
    settings.sensitivityY = std::clamp(settings.sensitivityY, 0.1f, 5.0f);
    settings.scrollSensitivity = std::clamp(settings.scrollSensitivity, 0.1f, 5.0f);
    settings.dragThreshold = std::clamp(settings.dragThreshold, 0.0f, 64.0f);
    settings.region.left = std::clamp(settings.region.left, 0.0f, 0.95f);
    settings.region.top = std::clamp(settings.region.top, 0.0f, 0.95f);
    settings.region.right = std::clamp(settings.region.right,
                                       settings.region.left + 0.05f, 1.0f);
    settings.region.bottom = std::clamp(settings.region.bottom,
                                        settings.region.top + 0.05f, 1.0f);
    return settings;
}

PointerMode pointerModeFromLegacyTouchscreen(bool touchscreenMouseMode) {
    return touchscreenMouseMode ? PointerMode::MultiTouch
                                : PointerMode::TrackpadNatural;
}

const char* pointerModeName(PointerMode mode) {
    switch (mode) {
    case PointerMode::MultiTouch: return "multitouch";
    case PointerMode::Absolute: return "absolute";
    case PointerMode::AbsoluteSwapped: return "absolute-swapped";
    case PointerMode::TrackpadNatural: return "natural-trackpad";
    case PointerMode::TrackpadGaming: return "gaming-trackpad";
    case PointerMode::Disabled: return "disabled";
    }
    return "disabled";
}

bool pointerModeFromName(std::string_view name, PointerMode& mode) {
    for (const auto candidate : {PointerMode::MultiTouch, PointerMode::Absolute,
                                 PointerMode::AbsoluteSwapped,
                                 PointerMode::TrackpadNatural,
                                 PointerMode::TrackpadGaming,
                                 PointerMode::Disabled}) {
        if (name == pointerModeName(candidate)) {
            mode = candidate;
            return true;
        }
    }
    return false;
}

bool isTrackpadMode(PointerMode mode) {
    return mode == PointerMode::TrackpadNatural ||
           mode == PointerMode::TrackpadGaming;
}

bool isAbsoluteMode(PointerMode mode) {
    return mode == PointerMode::Absolute || mode == PointerMode::AbsoluteSwapped;
}

} // namespace artemis::input
