#pragma once

#include <cstdint>

namespace artemis::host {

// Which codec profile a host advertises. Mirrors the Sunshine/GFE
// ServerCodecModeSupport bits in Limelight.h so this module needs no
// Moonlight include and stays portable enough to unit-test on its own.
enum class CodecKind : int {
    H264 = 0,
    H265 = 1,
    AV1 = 2,
};

// A host's advertised encode support, derived from ServerCodecModeSupport.
struct CodecCapabilities {
    bool h264 = true;         // every GameStream host encodes H.264
    bool hevc = false;        // HEVC Main (8-bit)
    bool hevcMain10 = false;  // HEVC Main10 (10-bit, HDR)
    bool av1 = false;         // AV1 Main (8-bit)
    bool av1Main10 = false;   // AV1 Main10 (10-bit, HDR)
};

// Decode a ServerCodecModeSupport bitmap (Limelight SCM_* flags) into the
// CodecCapabilities struct. Unknown bits are ignored; a zero mask still means
// H.264-only support.
CodecCapabilities decodeServerCodecMask(std::uint32_t mask);

// Whether a chosen codec+colour-depth request is actually advertised by the
// host. hdr requests a 10-bit profile for the codec.
bool supportsCodec(const CodecCapabilities& caps, CodecKind codec, bool hdr);

} // namespace artemis::host