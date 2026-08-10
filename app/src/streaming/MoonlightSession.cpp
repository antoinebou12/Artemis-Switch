#include "MoonlightSession.hpp"
#include "AVFrameHolder.hpp"
#include "GameStreamClient.hpp"
#include "InputManager.hpp"
#include "Settings.hpp"
#include "StreamProfileStore.hpp"
#include "borealis.hpp"
#include "../features/stream/AdvancedStreamOptionsStore.hpp"
#include "../features/stream/VideoCodecFormats.hpp"
#include <string.h>

static_assert(artemis::stream::kVideoFormatH264 == VIDEO_FORMAT_H264);
static_assert(artemis::stream::kVideoFormatH265 == VIDEO_FORMAT_H265);
static_assert(artemis::stream::kVideoFormatH265Main10 == VIDEO_FORMAT_H265_MAIN10);
static_assert(artemis::stream::kVideoFormatAv1Main8 == VIDEO_FORMAT_AV1_MAIN8);
static_assert(artemis::stream::kVideoFormatAv1Main10 == VIDEO_FORMAT_AV1_MAIN10);

// Moonlight-common-c explicitly negotiates encoder color range through
// STREAM_CONFIGURATION::colorRange. Keep the existing MoonlightSession source
// intact and intercept only its initialization boundary so Artemis full-range
// affects both the host encoder request and the Switch deko3D conversion path.
static void ArtemisInitializeStreamConfiguration(
    PSTREAM_CONFIGURATION streamConfig) {
    LiInitializeStreamConfiguration(streamConfig);

    const auto& options =
        artemis::stream::AdvancedStreamOptionsStore::instance().get();
    streamConfig->colorRange = options.forceFullRangeVideo
        ? COLOR_RANGE_FULL
        : COLOR_RANGE_LIMITED;
}

#define LiInitializeStreamConfiguration ArtemisInitializeStreamConfiguration
#include "MoonlightSessionLegacy.inc"
#undef LiInitializeStreamConfiguration
