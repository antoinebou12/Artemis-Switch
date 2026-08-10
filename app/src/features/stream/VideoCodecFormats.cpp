#include "VideoCodecFormats.hpp"

namespace artemis::stream {

int supportedVideoFormats(StreamVideoCodec codec, bool requestHdr) {
    switch (codec) {
    case StreamVideoCodec::H264:
        return kVideoFormatH264;
    case StreamVideoCodec::H265: {
        int formats = kVideoFormatH265;
        if (requestHdr)
            formats |= kVideoFormatH265Main10;
        return formats;
    }
    case StreamVideoCodec::AV1: {
        int formats = kVideoFormatAv1Main8;
        if (requestHdr)
            formats |= kVideoFormatAv1Main10;
        return formats;
    }
    }
    return 0;
}

} // namespace artemis::stream
