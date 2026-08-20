#pragma once

namespace artemis::video {

// Mirrors Settings::UpscalingMode without pulling Borealis/Settings into tests.
enum UpscalingModeValue : int {
    UpscalingOff = 0,
    UpscalingMetalFx = 1,
    UpscalingFsr1 = 2,
    UpscalingSgsr1 = 3,
    UpscalingNis = 4,
};

// Apple (non-tvOS) exposes Off / MetalFX / FSR1. Switch and other FSR-only
// backends expose Off / FSR1 so the selector never advertises MetalFX.
inline int upscaling_selector_index(int mode, bool metal_fx_choices) {
    if (metal_fx_choices)
        return mode;
    return mode == UpscalingOff ? 0 : 1;
}

inline int upscaling_mode_from_selector(int index, bool metal_fx_choices) {
    if (metal_fx_choices) {
        if (index == UpscalingMetalFx || index == UpscalingFsr1)
            return index;
        return UpscalingOff;
    }
    return index <= 0 ? UpscalingOff : UpscalingFsr1;
}

// Switch exposes every deko3d upscaler: Off / FSR1 / SGSR1 / NIS. The Settings
// tab and the in-stream overlay both drive the same setting, so they must offer
// the same list -- the overlay used to show only Off/FSR1, which silently
// reset a profile's SGSR1 or NIS choice to FSR1 when touched mid-stream.
inline int switch_upscaling_selector_index(int mode) {
    switch (mode) {
        case UpscalingFsr1: return 1;
        case UpscalingSgsr1: return 2;
        case UpscalingNis: return 3;
        default: return 0;
    }
}

inline int switch_upscaling_mode_from_selector(int index) {
    switch (index) {
        case 1: return UpscalingFsr1;
        case 2: return UpscalingSgsr1;
        case 3: return UpscalingNis;
        default: return UpscalingOff;
    }
}

inline bool upscaling_active(int mode) { return mode != UpscalingOff; }

} // namespace artemis::video
