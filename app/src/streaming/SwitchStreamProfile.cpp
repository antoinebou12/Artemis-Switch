#include "SwitchStreamProfile.hpp"

#include <algorithm>
#include <array>

namespace artemis::streaming {
namespace {

bool validDecoderThreads(int threads) {
    constexpr std::array<int, 4> allowed{0, 2, 3, 4};
    return std::find(allowed.begin(), allowed.end(), threads) != allowed.end();
}

bool validCodec(const std::string& codec) {
    return codec == "H264" || codec == "HEVC" || codec == "H265";
}

} // namespace

ValidationResult SwitchStreamProfile::validate(const StreamProfile& profile) {
    ValidationResult result;
    auto fail = [&](std::string message) {
        result.valid = false;
        result.errors.push_back(std::move(message));
    };

    if (profile.width < 640 || profile.width > 1920)
        fail("width must be between 640 and 1920");
    if (profile.height < 360 || profile.height > 1080)
        fail("height must be between 360 and 1080");
    if (profile.fps != 30 && profile.fps != 40 && profile.fps != 60)
        fail("fps must be 30, 40, or 60");
    if (profile.bitrateKbps < 1000 || profile.bitrateKbps > 100000)
        fail("bitrate must be between 1 and 100 Mbps");
    if (!validDecoderThreads(profile.decoderThreads))
        fail("decoder threads must be 0, 2, 3, or 4");
    if (!validCodec(profile.codec))
        fail("codec must be H264 or HEVC");

    return result;
}

StreamProfile SwitchStreamProfile::normalized(StreamProfile profile) {
    profile.width = std::clamp(profile.width, 640, 1920);
    profile.height = std::clamp(profile.height, 360, 1080);
    profile.bitrateKbps = std::clamp(profile.bitrateKbps, 1000, 100000);
    profile.fps = profile.fps <= 30 ? 30 : (profile.fps <= 40 ? 40 : 60);
    if (!validDecoderThreads(profile.decoderThreads)) profile.decoderThreads = 2;
    if (profile.codec == "H265") profile.codec = "HEVC";
    if (!validCodec(profile.codec)) profile.codec = "HEVC";
    return profile;
}

} // namespace artemis::streaming
