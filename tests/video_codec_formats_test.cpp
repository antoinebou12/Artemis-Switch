#include "../app/src/features/stream/VideoCodecFormats.hpp"

#include <cassert>

using artemis::stream::kVideoFormatAv1Main10;
using artemis::stream::kVideoFormatAv1Main8;
using artemis::stream::kVideoFormatH264;
using artemis::stream::kVideoFormatH265;
using artemis::stream::kVideoFormatH265Main10;
using artemis::stream::StreamVideoCodec;
using artemis::stream::supportedVideoFormats;

int main() {
    assert(supportedVideoFormats(StreamVideoCodec::H264, false) == kVideoFormatH264);
    assert(supportedVideoFormats(StreamVideoCodec::H264, true) == kVideoFormatH264);

    assert(supportedVideoFormats(StreamVideoCodec::H265, false) == kVideoFormatH265);
    assert(supportedVideoFormats(StreamVideoCodec::H265, true) ==
           (kVideoFormatH265 | kVideoFormatH265Main10));

    assert(supportedVideoFormats(StreamVideoCodec::AV1, false) == kVideoFormatAv1Main8);
    assert(supportedVideoFormats(StreamVideoCodec::AV1, true) ==
           (kVideoFormatAv1Main8 | kVideoFormatAv1Main10));

    return 0;
}
