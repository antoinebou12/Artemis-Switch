#pragma once

namespace artemis::streaming {

enum class FsrPreset : int {
    Off = 0,
    Performance = 1,
    Balanced = 2,
    Quality = 3,
};

// Matches UpscalingMode::UPSCALING_FSR1 in Settings.hpp.
inline constexpr int kFsrPresetUpscalingFsr1 = 2;

inline FsrPreset normalizeFsrPreset(int value) {
    switch (value) {
    case static_cast<int>(FsrPreset::Performance):
    case static_cast<int>(FsrPreset::Balanced):
    case static_cast<int>(FsrPreset::Quality):
        return static_cast<FsrPreset>(value);
    default:
        return FsrPreset::Off;
    }
}

struct FsrPresetValues {
    int upscalingMode = kFsrPresetUpscalingFsr1;
    bool rcas = true;
    float rcasStrength = 0.20f;
};

inline bool fsrPresetValues(FsrPreset preset, FsrPresetValues* out) {
    if (!out)
        return false;
    switch (preset) {
    case FsrPreset::Performance:
        out->upscalingMode = kFsrPresetUpscalingFsr1;
        out->rcas = true;
        out->rcasStrength = 0.50f;
        return true;
    case FsrPreset::Balanced:
        out->upscalingMode = kFsrPresetUpscalingFsr1;
        out->rcas = true;
        out->rcasStrength = 0.35f;
        return true;
    case FsrPreset::Quality:
        out->upscalingMode = kFsrPresetUpscalingFsr1;
        out->rcas = true;
        out->rcasStrength = 0.20f;
        return true;
    case FsrPreset::Off:
    default:
        return false;
    }
}

// Off is a no-op: pointers are left unchanged and the return is false.
inline bool applyFsrPreset(int preset, int* upscalingMode, bool* rcas,
                           float* rcasStrength) {
    FsrPresetValues values;
    if (!fsrPresetValues(normalizeFsrPreset(preset), &values))
        return false;
    if (upscalingMode)
        *upscalingMode = values.upscalingMode;
    if (rcas)
        *rcas = values.rcas;
    if (rcasStrength)
        *rcasStrength = values.rcasStrength;
    return true;
}

inline bool fsrPresetSelectable(int upscalingMode) {
    return upscalingMode != 0;
}

} // namespace artemis::streaming
