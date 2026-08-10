#include "StreamConfigProfileNormalize.hpp"

#include <cassert>

using namespace artemis::streaming;

int main() {
    assert(normalizeProfileHeight(360) == 360);
    assert(normalizeProfileHeight(480) == 480);
    assert(normalizeProfileHeight(720) == 720);
    assert(normalizeProfileHeight(1080) == 1080);
    assert(normalizeProfileHeight(700) == 720);
    assert(normalizeProfileHeight(400) == 360);
    assert(normalizeProfileHeight(900) == 1080);
    assert(normalizeProfileHeight(0) == 720);
    assert(normalizeProfileHeight(-10) == 360);

    ProfileNormalizeInput profile;
    profile.resolutionHeight = 999;
    profile.fps = 0;
    profile.bitrateKbps = -5;
    profile.mouseSpeedMultiplier = 200;
    profile.deadzoneLeft = 2.0f;
    profile.rumbleForce = 5.0f;
    profile.name.clear();
    profile = normalizeProfileFields(profile);
    assert(profile.resolutionHeight == 1080);
    assert(profile.fps == 60);
    assert(profile.bitrateKbps == 10000);
    assert(profile.mouseSpeedMultiplier == 100);
    assert(profile.deadzoneLeft <= 1.0f + 1e-6f);
    assert(profile.rumbleForce <= 1.0f + 1e-6f);
    assert(profile.name == "Profile");

    return 0;
}
