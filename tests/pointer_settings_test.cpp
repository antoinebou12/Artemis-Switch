#include "../app/src/features/input/PointerSettings.hpp"

#include <cassert>

using namespace artemis::input;

int main() {
    assert(pointerModeFromLegacyTouchscreen(true) == PointerMode::MultiTouch);
    assert(pointerModeFromLegacyTouchscreen(false) == PointerMode::TrackpadNatural);
    assert(isTrackpadMode(PointerMode::TrackpadGaming));
    assert(!isTrackpadMode(PointerMode::Absolute));
    assert(isAbsoluteMode(PointerMode::AbsoluteSwapped));

    PointerMode parsed = PointerMode::Disabled;
    assert(pointerModeFromName("multitouch", parsed));
    assert(parsed == PointerMode::MultiTouch);
    assert(!pointerModeFromName("shell-command", parsed));

    PointerSettings invalid;
    invalid.sensitivityX = -4.0f;
    invalid.sensitivityY = 20.0f;
    invalid.scrollSensitivity = 0.0f;
    invalid.dragThreshold = 500.0f;
    invalid.region = {0.9f, 0.9f, 0.1f, 0.1f};
    const auto safe = validatePointerSettings(invalid);
    assert(safe.sensitivityX == 0.1f);
    assert(safe.sensitivityY == 5.0f);
    assert(safe.scrollSensitivity == 0.1f);
    assert(safe.dragThreshold == 64.0f);
    assert(safe.region.right >= safe.region.left + 0.05f);
    assert(safe.region.bottom >= safe.region.top + 0.05f);
}
