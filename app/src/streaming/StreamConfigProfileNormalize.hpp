#pragma once

#include <algorithm>
#include <cmath>
#include <string>

namespace artemis::streaming {

inline const char* profileStoreFilename() { return "profile.json"; }

inline const char* legacyProfileStoreFilename() {
    return "artemis_profiles.json";
}

inline int normalizeProfileHeight(int height) {
    if (height == -1)
        return -1;
    constexpr int allowed[] = {360, 480, 540, 720, 1080, 1440};
    int best = 720;
    int bestDistance = std::abs(height - best);
    for (int value : allowed) {
        const int distance = std::abs(height - value);
        if (distance < bestDistance) {
            best = value;
            bestDistance = distance;
        }
    }
    return best;
}

// 1440p (and native 2.0x) is more decode work than the Switch panel. Warn in UI.
inline bool highResolutionMayLag(int resolutionHeight,
                                 int nativeScalePercent = 100) {
    if (resolutionHeight >= 1440)
        return true;
    return resolutionHeight < 0 && nativeScalePercent >= 200;
}

inline int normalizeNativeResolutionScale(int scale) {
    switch (scale) {
    case 50:
    case 75:
    case 100:
    case 200:
        return scale;
    default:
        return 100;
    }
}

inline int normalizeCustomDimension(int value, int fallback) {
    if (value < 160)
        return fallback;
    if (value > 7680)
        return 7680;
    return value;
}

inline int normalizeVolume(int volume) {
    return std::clamp(volume, 0, 100);
}

struct ProfileNormalizeInput {
    std::string name;
    int resolutionHeight = 720;
    int fps = 60;
    int bitrateKbps = 10000;
    int mouseSpeedMultiplier = 47;
    float deadzoneLeft = 0.0f;
    float deadzoneRight = 0.0f;
    float rumbleForce = 1.0f;
};

inline ProfileNormalizeInput
normalizeProfileFields(ProfileNormalizeInput profile) {
    profile.resolutionHeight = normalizeProfileHeight(profile.resolutionHeight);
    if (profile.fps <= 0)
        profile.fps = 60;
    if (profile.bitrateKbps <= 0)
        profile.bitrateKbps = 10000;
    profile.mouseSpeedMultiplier =
        std::clamp(profile.mouseSpeedMultiplier, 0, 100);
    profile.deadzoneLeft = std::clamp(profile.deadzoneLeft, 0.0f, 1.0f);
    profile.deadzoneRight = std::clamp(profile.deadzoneRight, 0.0f, 1.0f);
    profile.rumbleForce = std::clamp(profile.rumbleForce, 0.0f, 1.0f);
    if (profile.name.empty())
        profile.name = "Profile";
    return profile;
}

} // namespace artemis::streaming
