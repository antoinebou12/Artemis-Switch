#include "SwitchStreamProfile.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cmath>

namespace artemis::streaming {
namespace {

bool validDecoderThreads(int threads) {
    constexpr std::array<int, 4> allowed{0, 2, 3, 4};
    return std::find(allowed.begin(), allowed.end(), threads) != allowed.end();
}

bool validFps(int fps) {
    constexpr std::array<int, 5> allowed{30, 40, 60, 90, 120};
    return std::find(allowed.begin(), allowed.end(), fps) != allowed.end();
}

bool validCodec(const std::string& codec) {
    return codec == "H264" || codec == "HEVC" || codec == "H265";
}

int nearestFps(int requested) {
    constexpr std::array<int, 5> allowed{30, 40, 60, 90, 120};
    return *std::min_element(allowed.begin(), allowed.end(), [requested](int a, int b) {
        return std::abs(a - requested) < std::abs(b - requested);
    });
}

} // namespace

ValidationResult SwitchStreamProfile::validate(const StreamProfile& profile) {
    ValidationResult result;
    auto fail = [&](std::string message) {
        result.valid = false;
        result.errors.push_back(std::move(message));
    };

    if (profile.width < 360 || profile.width > 1920)
        fail("width must be between 360 and 1920");
    if (profile.height < 360 || profile.height > 1920)
        fail("height must be between 360 and 1920");
    if (static_cast<long long>(profile.width) * profile.height > 1920LL * 1080LL)
        fail("resolution exceeds the Switch 1080p decode pixel budget");
    if (!validFps(profile.fps))
        fail("fps must be 30, 40, 60, 90, or 120");
    if (profile.bitrateKbps < 1000 || profile.bitrateKbps > 100000)
        fail("bitrate must be between 1 and 100 Mbps");
    if (!validDecoderThreads(profile.decoderThreads))
        fail("decoder threads must be 0, 2, 3, or 4");
    if (!validCodec(profile.codec))
        fail("codec must be H264 or HEVC");

    return result;
}

StreamProfile SwitchStreamProfile::normalized(StreamProfile profile) {
    profile.width = std::clamp(profile.width, 360, 1920);
    profile.height = std::clamp(profile.height, 360, 1920);
    constexpr double pixelBudget = 1920.0 * 1080.0;
    const double pixels = static_cast<double>(profile.width) * profile.height;
    if (pixels > pixelBudget) {
        const double scale = std::sqrt(pixelBudget / pixels);
        profile.width = std::max(360, static_cast<int>(profile.width * scale) & ~1);
        profile.height = std::max(360, static_cast<int>(profile.height * scale) & ~1);
    }
    profile.bitrateKbps = std::clamp(profile.bitrateKbps, 1000, 100000);
    if (!validFps(profile.fps))
        profile.fps = nearestFps(profile.fps);
    if (!validDecoderThreads(profile.decoderThreads)) profile.decoderThreads = 2;
    if (profile.codec == "H265") profile.codec = "HEVC";
    if (!validCodec(profile.codec)) profile.codec = "HEVC";
    return profile;
}

} // namespace artemis::streaming
