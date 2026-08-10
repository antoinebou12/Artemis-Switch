#pragma once

#include <cstdint>

namespace artemis::stream {

// Moonlight VIDEO_FORMAT_* values (Limelight.h) — kept local so unit tests
// do not need the full GameStream headers.
constexpr int kVideoFormatH264 = 0x0001;
constexpr int kVideoFormatH265 = 0x0100;
constexpr int kVideoFormatH265Main10 = 0x0200;
constexpr int kVideoFormatAv1Main8 = 0x1000;
constexpr int kVideoFormatAv1Main10 = 0x2000;

// Mirrors Settings::VideoCodec (H264=0, H265=1, AV1=2).
enum class StreamVideoCodec : int { H264 = 0, H265 = 1, AV1 = 2 };

// Builds the STREAM_CONFIGURATION::supportedVideoFormats mask for a session.
int supportedVideoFormats(StreamVideoCodec codec, bool requestHdr);

} // namespace artemis::stream
