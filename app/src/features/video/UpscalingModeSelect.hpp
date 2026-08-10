#pragma once

namespace artemis::video {

// Mirrors Settings::UpscalingMode without pulling Borealis/Settings into tests.
enum UpscalingModeValue : int {
    UpscalingOff = 0,
    UpscalingMetalFx = 1,
    UpscalingFsr1 = 2,
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

inline bool upscaling_active(int mode) { return mode != UpscalingOff; }

} // namespace artemis::video
