#include "CodecCapabilities.hpp"

namespace artemis::host {

// ServerCodecModeSupport bit values, kept in sync with Limelight.h SCM_*
// (scoped here so the module needs no Moonlight header to build standalone).
namespace {
constexpr std::uint32_t kScmH264 = 0x00000001;
constexpr std::uint32_t kScmH264High8 = 0x00040000;
constexpr std::uint32_t kScmHevc = 0x00000100;
constexpr std::uint32_t kScmHevcMain10 = 0x00000200;
constexpr std::uint32_t kScmHevcRext8 = 0x00080000;
constexpr std::uint32_t kScmHevcRext10 = 0x00100000;
constexpr std::uint32_t kScmAv1Main8 = 0x00010000;
constexpr std::uint32_t kScmAv1Main10 = 0x00020000;
constexpr std::uint32_t kScmAv1High8 = 0x00200000;
constexpr std::uint32_t kScmAv1High10 = 0x00400000;

constexpr std::uint32_t kMaskHevc =
    kScmHevc | kScmHevcMain10 | kScmHevcRext8 | kScmHevcRext10;
constexpr std::uint32_t kMaskHevc10 = kScmHevcMain10 | kScmHevcRext10;
constexpr std::uint32_t kMaskAv1 =
    kScmAv1Main8 | kScmAv1Main10 | kScmAv1High8 | kScmAv1High10;
constexpr std::uint32_t kMaskAv110 = kScmAv1Main10 | kScmAv1High10;
} // namespace

CodecCapabilities decodeServerCodecMask(std::uint32_t mask) {
    CodecCapabilities caps;
    // H.264 is the baseline: every supported GameStream host can encode it,
    // even when the mask is zero (older GFE reports no flags for H.264).
    caps.h264 = true;
    caps.hevc = (mask & kScmHevc) != 0;
    caps.hevcMain10 = (mask & kScmHevcMain10) != 0;
    caps.av1 = (mask & kScmAv1Main8) != 0;
    caps.av1Main10 = (mask & kScmAv1Main10) != 0;
    return caps;
}

bool supportsCodec(const CodecCapabilities& caps, CodecKind codec, bool hdr) {
    switch (codec) {
        case CodecKind::H264:
            // H.264 is always available. hdr is never requested with H.264.
            return caps.h264;
        case CodecKind::H265:
            if (hdr)
                return caps.hevcMain10;
            return caps.hevc;
        case CodecKind::AV1:
            if (hdr)
                return caps.av1Main10;
            return caps.av1;
    }
    return false;
}

} // namespace artemis::host