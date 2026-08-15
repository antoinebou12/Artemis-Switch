#include "DefaultStreamProfiles.hpp"
#include "FsrPreset.hpp"
#include "StreamAspectRatio.hpp"
#include "StreamConfigProfileNormalize.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <set>
#include <string>
#include <vector>

using namespace artemis::streaming;

int main() {
    assert(std::strcmp(profileStoreFilename(), "profile.json") == 0);
    assert(std::strcmp(legacyProfileStoreFilename(),
                       "artemis_profiles.json") == 0);

    assert(streamWidthFromHeight(720, StreamAspectRatio::Ratio16x9) == 1280);
    assert(streamWidthFromHeight(1080, StreamAspectRatio::Ratio16x9) == 1920);
    assert(streamWidthFromHeight(1440, StreamAspectRatio::Ratio16x9) == 2560);
    assert(streamWidthFromHeight(1440, StreamAspectRatio::Ratio4x3) == 1920);
    assert(streamWidthFromHeight(720, StreamAspectRatio::Ratio4x3) == 960);
    assert(streamWidthFromHeight(1080, StreamAspectRatio::Ratio4x3) == 1440);
    assert(aspectRatioFromString("4:3") == StreamAspectRatio::Ratio4x3);
    assert(aspectRatioFromString("16:9") == StreamAspectRatio::Ratio16x9);
    assert(std::strcmp(aspectRatioToString(StreamAspectRatio::Ratio4x3),
                       "4:3") == 0);

    assert(normalizeProfileHeight(360) == 360);
    assert(normalizeProfileHeight(480) == 480);
    assert(normalizeProfileHeight(720) == 720);
    assert(normalizeProfileHeight(1080) == 1080);
    assert(normalizeProfileHeight(1440) == 1440);
    assert(normalizeProfileHeight(1400) == 1440);
    assert(!highResolutionMayLag(720));
    assert(!highResolutionMayLag(1080));
    assert(highResolutionMayLag(1440));
    assert(highResolutionMayLag(-1, 200));
    assert(!highResolutionMayLag(-1, 100));
    assert(normalizeProfileHeight(700) == 720);
    assert(normalizeProfileHeight(400) == 360);
    assert(normalizeProfileHeight(900) == 720);
    assert(normalizeProfileHeight(0) == 360);
    assert(normalizeProfileHeight(-1) == -1);
    assert(normalizeProfileHeight(-10) == 360);

    assert(normalizeNativeResolutionScale(50) == 50);
    assert(normalizeNativeResolutionScale(75) == 75);
    assert(normalizeNativeResolutionScale(100) == 100);
    assert(normalizeNativeResolutionScale(200) == 200);
    assert(normalizeNativeResolutionScale(33) == 100);
    assert(normalizeCustomDimension(80, 1920) == 1920);
    assert(normalizeCustomDimension(1280, 1920) == 1280);
    assert(normalizeCustomDimension(9000, 1920) == 7680);

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

    assert(kDefaultStreamProfiles.size() == 18);
    assert(std::strcmp(kDefaultActiveProfileName, "720p 60 10M") == 0);
    assert(isDefaultProfileName(kDefaultActiveProfileName));

    std::set<std::string> names;
    std::set<int> bitrates;
    bool foundLowLatency = false;
    for (const auto& spec : kDefaultStreamProfiles) {
        assert(spec.fps == 30 || spec.fps == 60);
        assert(spec.height == 360 || spec.height == 480 || spec.height == 540 ||
               spec.height == 720 || spec.height == 1080 || spec.height == 1440);
        names.insert(spec.name);
        bitrates.insert(spec.bitrateKbps);
        if (std::strcmp(spec.name, "720p 60 Low Latency") == 0) {
            foundLowLatency = true;
            assert(spec.lowLatencyPacing);
            assert(spec.framesQueueSize == 2);
        }
    }
    assert(foundLowLatency);
    assert(names.size() == 18);
    assert(names.count("360p 30 0.5M") == 1);
    assert(names.count("360p 30 1M") == 1);
    assert(names.count("480p 30 5M") == 1);
    assert(names.count("480p 60 10M") == 1);
    assert(names.count("540p 30 5M") == 1);
    assert(names.count("540p 60 10M") == 1);
    assert(names.count("720p 30 10M") == 1);
    assert(names.count("720p 60 10M") == 1);
    assert(names.count("720p 60 20M") == 1);
    assert(names.count("720p 60 Low Latency") == 1);
    assert(names.count("1080p 30 20M") == 1);
    assert(names.count("1080p 60 20M") == 1);
    assert(names.count("1080p 60 50M") == 1);
    assert(names.count("1080p 60 100M") == 1);
    assert(names.count("1440p 30 20M") == 1);
    assert(names.count("1440p 30 50M") == 1);
    assert(names.count("1440p 60 50M") == 1);
    assert(names.count("1440p 60 100M") == 1);
    assert(names.count("720p 4:3") == 0);
    assert(names.count("360p remote") == 0);
    const std::set<int> expectedBitrates{500, 1000, 5000, 10000, 20000, 50000,
                                         100000};
    assert(bitrates == expectedBitrates);

    std::vector<std::string> existing;
    for (const auto& spec : kDefaultStreamProfiles)
        existing.emplace_back(spec.name);
    assert(missingDefaultProfiles(existing).empty());

    existing.erase(
        std::remove(existing.begin(), existing.end(), std::string("720p 60 10M")),
        existing.end());
    const auto missing = missingDefaultProfiles(existing);
    assert(missing.size() == 1);
    assert(std::strcmp(missing.front().name, "720p 60 10M") == 0);

    assert(normalizeFsrPreset(0) == FsrPreset::Off);
    assert(normalizeFsrPreset(99) == FsrPreset::Off);
    assert(normalizeFsrPreset(-1) == FsrPreset::Off);
    assert(normalizeFsrPreset(1) == FsrPreset::Performance);
    assert(normalizeFsrPreset(2) == FsrPreset::Balanced);
    assert(normalizeFsrPreset(3) == FsrPreset::Quality);
    assert(!fsrPresetSelectable(0));
    assert(fsrPresetSelectable(kFsrPresetUpscalingFsr1));

    int mode = 0;
    bool rcas = false;
    float strength = 0.0f;
    assert(!applyFsrPreset(static_cast<int>(FsrPreset::Off), &mode, &rcas,
                           &strength));
    assert(mode == 0);
    assert(!rcas);
    assert(strength == 0.0f);

    assert(applyFsrPreset(static_cast<int>(FsrPreset::Performance), &mode, &rcas,
                          &strength));
    assert(mode == kFsrPresetUpscalingFsr1);
    assert(rcas);
    assert(strength == 0.50f);

    assert(applyFsrPreset(static_cast<int>(FsrPreset::Balanced), &mode, &rcas,
                          &strength));
    assert(mode == kFsrPresetUpscalingFsr1);
    assert(rcas);
    assert(strength == 0.35f);

    assert(applyFsrPreset(static_cast<int>(FsrPreset::Quality), &mode, &rcas,
                          &strength));
    assert(mode == kFsrPresetUpscalingFsr1);
    assert(rcas);
    assert(strength == 0.20f);

    return 0;
}
