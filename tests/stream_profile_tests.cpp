#include "SwitchStreamProfile.hpp"

#include <cassert>

using namespace artemis::streaming;

int main() {
    StreamProfile valid{1920, 1080, 60, 20000, 2, "HEVC"};
    auto validation = SwitchStreamProfile::validate(valid);
    assert(validation.valid);
    assert(validation.errors.empty());

    StreamProfile invalid{4000, 2000, 120, 250000, 9, "AV1"};
    validation = SwitchStreamProfile::validate(invalid);
    assert(!validation.valid);
    assert(validation.errors.size() >= 5);

    StreamProfile normal{500, 2000, 55, 500, 9, "H265"};
    normal = SwitchStreamProfile::normalized(normal);
    assert(normal.width == 640);
    assert(normal.height == 1080);
    assert(normal.fps == 60);
    assert(normal.bitrateKbps == 1000);
    assert(normal.decoderThreads == 2);
    assert(normal.codec == "HEVC");

    return 0;
}
