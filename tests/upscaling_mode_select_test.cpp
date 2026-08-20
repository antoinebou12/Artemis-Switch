#include "UpscalingModeSelect.hpp"

#include <cassert>
#include <iostream>

using artemis::video::switch_upscaling_mode_from_selector;
using artemis::video::switch_upscaling_selector_index;
using artemis::video::UpscalingFsr1;
using artemis::video::UpscalingNis;
using artemis::video::UpscalingSgsr1;
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

    // Switch offers every deko3d upscaler; the Settings tab and the in-stream
    // overlay must agree on this list or the overlay silently downgrades a
    // profile's SGSR1/NIS choice to FSR1.
    assert(switch_upscaling_selector_index(UpscalingOff) == 0);
    assert(switch_upscaling_selector_index(UpscalingFsr1) == 1);
    assert(switch_upscaling_selector_index(UpscalingSgsr1) == 2);
    assert(switch_upscaling_selector_index(UpscalingNis) == 3);
    // MetalFX is never offered on Switch and must fall back to Off.
    assert(switch_upscaling_selector_index(UpscalingMetalFx) == 0);

    assert(switch_upscaling_mode_from_selector(0) == UpscalingOff);
    assert(switch_upscaling_mode_from_selector(1) == UpscalingFsr1);
    assert(switch_upscaling_mode_from_selector(2) == UpscalingSgsr1);
    assert(switch_upscaling_mode_from_selector(3) == UpscalingNis);
    assert(switch_upscaling_mode_from_selector(99) == UpscalingOff);
    assert(switch_upscaling_mode_from_selector(-1) == UpscalingOff);

    // Round trip for every mode the Switch selector can show.
    for (int mode : {UpscalingOff, UpscalingFsr1, UpscalingSgsr1, UpscalingNis}) {
        assert(switch_upscaling_mode_from_selector(
                   switch_upscaling_selector_index(mode)) == mode);
    }

    assert(upscaling_active(UpscalingSgsr1));
    assert(upscaling_active(UpscalingNis));

    std::cout << "upscaling_mode_select_test ok\n";
    return 0;
}
