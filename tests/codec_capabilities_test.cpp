#include "../app/src/features/host/CodecCapabilities.hpp"

#include <cassert>
#include <cstdint>

using artemis::host::CodecCapabilities;
using artemis::host::CodecKind;
using artemis::host::decodeServerCodecMask;
using artemis::host::supportsCodec;

namespace {

// Resend known ServerCodecModeSupport masks from real hosts.
// GFE is H.264 + HEVC Main; Sunshine and Apollo advertise the 10-bit/AV1
// extensions on top.
constexpr std::uint32_t kGfeH264Only = 0x00000001;
constexpr std::uint32_t kGfeH264Hevc = 0x00000101;
constexpr std::uint32_t kApolloSdr =
    0x00000001 | 0x00000100 | 0x00000200;  // H.264 + HEVC + HEVC Main10
constexpr std::uint32_t kSunshineSdr =
    0x00000001 | 0x00000100 | 0x00010000;  // H.264 + HEVC (8-bit) + AV1 8-bit
constexpr std::uint32_t kSunshineHdr =
    kSunshineSdr | 0x00000200 | 0x00020000;  // + HEVC Main10 + AV1 Main10

void oldGfeIsH264Only() {
    const auto caps = decodeServerCodecMask(kGfeH264Only);
    assert(caps.h264);
    assert(!caps.hevc);
    assert(!caps.hevcMain10);
    assert(!caps.av1);
    assert(!caps.av1Main10);
    assert(supportsCodec(caps, CodecKind::H264, false));
    assert(!supportsCodec(caps, CodecKind::H265, false));
    assert(!supportsCodec(caps, CodecKind::H265, true));
    assert(!supportsCodec(caps, CodecKind::AV1, false));
}

void modernGfeAddsHevc() {
    const auto caps = decodeServerCodecMask(kGfeH264Hevc);
    assert(caps.h264);
    assert(caps.hevc);
    assert(!caps.hevcMain10);
    assert(supportsCodec(caps, CodecKind::H265, false));
    // 10-bit/HDR still unsupported even though 8-bit HEVC is present.
    assert(!supportsCodec(caps, CodecKind::H265, true));
}

void apolloSdr() {
    const auto caps = decodeServerCodecMask(kApolloSdr);
    assert(caps.hevc);
    assert(caps.hevcMain10);
    assert(!caps.av1);
    assert(supportsCodec(caps, CodecKind::H265, true));
    assert(supportsCodec(caps, CodecKind::H265, false));
    assert(!supportsCodec(caps, CodecKind::AV1, false));
}

void sunshineSdr() {
    const auto caps = decodeServerCodecMask(kSunshineSdr);
    assert(caps.hevc);
    assert(caps.av1);
    assert(!caps.hevcMain10);
    assert(!caps.av1Main10);
    assert(supportsCodec(caps, CodecKind::AV1, false));
    assert(!supportsCodec(caps, CodecKind::AV1, true));
    assert(supportsCodec(caps, CodecKind::H265, false));
    assert(!supportsCodec(caps, CodecKind::H265, true));
}

void sunshineHdr() {
    const auto caps = decodeServerCodecMask(kSunshineHdr);
    assert(caps.hevc);
    assert(caps.hevcMain10);
    assert(caps.av1);
    assert(caps.av1Main10);
    assert(supportsCodec(caps, CodecKind::AV1, true));
    assert(supportsCodec(caps, CodecKind::H265, true));
    assert(supportsCodec(caps, CodecKind::H264, false));
}

void zeroMaskIsH264Only() {
    // Older GFE omitted flags entirely; H.264 must still be assumed.
    const auto caps = decodeServerCodecMask(0);
    assert(caps.h264);
    assert(!caps.hevc);
    assert(!caps.av1);
    assert(supportsCodec(caps, CodecKind::H264, false));
    assert(!supportsCodec(caps, CodecKind::H265, false));
}

void allBits() {
    const auto caps = decodeServerCodecMask(0xFFFFFFFF);
    assert(caps.h264 && caps.hevc && caps.hevcMain10);
    assert(caps.av1 && caps.av1Main10);
    assert(supportsCodec(caps, CodecKind::AV1, true));
    assert(supportsCodec(caps, CodecKind::H265, true));
}

} // namespace

int main() {
    oldGfeIsH264Only();
    modernGfeAddsHevc();
    apolloSdr();
    sunshineSdr();
    sunshineHdr();
    zeroMaskIsH264Only();
    allBits();
    return 0;
}