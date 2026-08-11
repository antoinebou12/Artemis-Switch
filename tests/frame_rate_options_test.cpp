#include "../app/src/features/stream/FrameRateOptions.hpp"

#include <cassert>
#include <string>

using artemis::stream::availableFrameRatePresets;
using artemis::stream::frameRatePresetIndex;
using artemis::stream::launchModeFpsValue;
using artemis::stream::resolveClientRefreshRateX100;

int main() {
    const auto presets = availableFrameRatePresets();
    assert(presets.size() >= 5);

    assert(resolveClientRefreshRateX100(60, 0) == 6000);
    assert(resolveClientRefreshRateX100(60, 5994) == 5994);
    assert(resolveClientRefreshRateX100(120, 11988) == 11988);

    assert(launchModeFpsValue(60, 0, false) == 60);
    assert(launchModeFpsValue(60, 0, true) == 60);
    assert(launchModeFpsValue(60, 5994, true) == 59940);
    assert(launchModeFpsValue(120, 11988, true) == 119880);
    // Sunshine / GFE keep integer mode fps even when NTSC x100 is set.
    assert(launchModeFpsValue(60, 5994, false) == 60);

    assert(frameRatePresetIndex(60, 0) >= 0);
    assert(frameRatePresetIndex(60, 5994) >= 0);
    assert(presets[static_cast<size_t>(frameRatePresetIndex(60, 5994))]
               .clientRefreshRateX100 == 5994);

    return 0;
}
