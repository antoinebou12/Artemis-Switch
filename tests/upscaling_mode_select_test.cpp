#include "UpscalingModeSelect.hpp"

#include <cassert>
#include <iostream>

using artemis::video::UpscalingFsr1;
using artemis::video::UpscalingMetalFx;
using artemis::video::UpscalingOff;
using artemis::video::upscaling_active;
using artemis::video::upscaling_mode_from_selector;
using artemis::video::upscaling_selector_index;

int main() {
    assert(upscaling_selector_index(UpscalingOff, false) == 0);
    assert(upscaling_selector_index(UpscalingMetalFx, false) == 1);
    assert(upscaling_selector_index(UpscalingFsr1, false) == 1);
    assert(upscaling_mode_from_selector(0, false) == UpscalingOff);
    assert(upscaling_mode_from_selector(1, false) == UpscalingFsr1);
    assert(upscaling_mode_from_selector(2, false) == UpscalingFsr1);

    assert(upscaling_selector_index(UpscalingOff, true) == 0);
    assert(upscaling_selector_index(UpscalingMetalFx, true) == 1);
    assert(upscaling_selector_index(UpscalingFsr1, true) == 2);
    assert(upscaling_mode_from_selector(0, true) == UpscalingOff);
    assert(upscaling_mode_from_selector(1, true) == UpscalingMetalFx);
    assert(upscaling_mode_from_selector(2, true) == UpscalingFsr1);
    assert(upscaling_mode_from_selector(99, true) == UpscalingOff);

    assert(!upscaling_active(UpscalingOff));
    assert(upscaling_active(UpscalingFsr1));
    assert(upscaling_active(UpscalingMetalFx));

    std::cout << "upscaling_mode_select_test ok\n";
    return 0;
}
