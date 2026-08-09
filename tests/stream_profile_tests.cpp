#include "SwitchStreamProfile.hpp"

#include <cassert>

using namespace artemis::streaming;

int main() {
    StreamProfile valid{1920, 1080, 60, 20000, 2, "HEVC"};
    auto validation = SwitchStreamProfile::validate(valid);
    assert(validation.valid);
    assert(validation.errors.empty());

    // Boundary values accepted by the actual Switch settings model.
    StreamProfile minimum{640, 360, 30, 1000, 0, "H264"};
    assert(SwitchStreamProfile::validate(minimum).valid);
    StreamProfile maximum{1920, 1080, 120, 100000, 4, "HEVC"};
    assert(SwitchStreamProfile::validate(maximum).valid);

    StreamProfile high90{1920, 1080, 90, 25000, 2, "HEVC"};
    assert(SwitchStreamProfile::validate(high90).valid);
    StreamProfile high120{1280, 720, 120, 20000, 2, "H264"};
    assert(SwitchStreamProfile::validate(high120).valid);

    // H265 is accepted as an alias and normalized to the canonical HEVC name.
    StreamProfile h265Alias{1280, 720, 60, 7000, 3, "H265"};
    assert(SwitchStreamProfile::validate(h265Alias).valid);
    h265Alias = SwitchStreamProfile::normalized(h265Alias);
    assert(h265Alias.codec == "HEVC");

    StreamProfile invalid{4000, 2000, 77, 250000, 9, "AV1"};
    validation = SwitchStreamProfile::validate(invalid);
    assert(!validation.valid);
    assert(validation.errors.size() >= 6);

    StreamProfile normal{500, 2000, 55, 500, 9, "H265"};
    normal = SwitchStreamProfile::normalized(normal);
    assert(normal.width == 640);
    assert(normal.height == 1080);
    assert(normal.fps == 60);
    assert(normal.bitrateKbps == 1000);
    assert(normal.decoderThreads == 2);
    assert(normal.codec == "HEVC");

    StreamProfile near90{1920, 1080, 88, 20000, 2, "HEVC"};
    near90 = SwitchStreamProfile::normalized(near90);
    assert(near90.fps == 90);

    StreamProfile veryHigh{9000, 9000, 1000, 999999, -4, "unknown"};
    veryHigh = SwitchStreamProfile::normalized(veryHigh);
    assert(veryHigh.width == 1920);
    assert(veryHigh.height == 1080);
    assert(veryHigh.fps == 120);
    assert(veryHigh.bitrateKbps == 100000);
    assert(veryHigh.decoderThreads == 2);
    assert(veryHigh.codec == "HEVC");

    StreamProfile veryLow{-1, -1, -100, -1, 1, ""};
    veryLow = SwitchStreamProfile::normalized(veryLow);
    assert(veryLow.width == 640);
    assert(veryLow.height == 360);
    assert(veryLow.fps == 30);
    assert(veryLow.bitrateKbps == 1000);
    assert(veryLow.decoderThreads == 2);
    assert(veryLow.codec == "HEVC");

    return 0;
}
