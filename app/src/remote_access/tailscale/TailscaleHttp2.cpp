#include "TailscaleHttp2.hpp"

#include <algorithm>
#include <cstring>

namespace artemis::tailscale {

namespace {

constexpr std::size_t kHttp2HeaderLen = 9;

void write24(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

void write32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

std::uint32_t read24(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 16U) |
           (static_cast<std::uint32_t>(p[1]) << 8U) |
           static_cast<std::uint32_t>(p[2]);
}

std::uint32_t read32(const std::uint8_t* p) {
    return ((static_cast<std::uint32_t>(p[0]) & 0x7fU) << 24U) |
           (static_cast<std::uint32_t>(p[1]) << 16U) |
           (static_cast<std::uint32_t>(p[2]) << 8U) |
           static_cast<std::uint32_t>(p[3]);
}

} // namespace

std::vector<std::uint8_t> buildHttp2ClientPreface() {
    // 24 bytes connection preface + 9 bytes empty SETTINGS frame
    static const char kPreface[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    std::vector<std::uint8_t> out(reinterpret_cast<const std::uint8_t*>(kPreface),
                                  reinterpret_cast<const std::uint8_t*>(kPreface) + 24);
    // Initial empty SETTINGS frame (type 0x04, stream 0, length 0)
    write24(out, 0);
    out.push_back(0x04); // Type: SETTINGS
    out.push_back(0x00); // Flags: 0
    write32(out, 0);     // Stream ID: 0
    return out;
}

std::vector<std::uint8_t> buildHttp2SettingsAck() {
    std::vector<std::uint8_t> out;
    out.reserve(kHttp2HeaderLen);
    write24(out, 0);
    out.push_back(0x04); // Type: SETTINGS
    out.push_back(0x01); // Flags: ACK
    write32(out, 0);     // Stream ID: 0
    return out;
}

std::vector<std::uint8_t> buildHttp2PingAck(std::span<const std::uint8_t> opaqueData) {
    std::vector<std::uint8_t> out;
    const std::uint32_t len = static_cast<std::uint32_t>(opaqueData.size());
    out.reserve(kHttp2HeaderLen + len);
    write24(out, len);
    out.push_back(0x06); // Type: PING
    out.push_back(0x01); // Flags: ACK
    write32(out, 0);     // Stream ID: 0
    out.insert(out.end(), opaqueData.begin(), opaqueData.end());
    return out;
}

std::vector<std::uint8_t> buildHttp2PostHeaders(std::uint32_t streamId,
                                                std::string_view authority,
                                                std::string_view path) {
    // HPACK block:
    // :method: POST (index 3 -> 0x83)
    // :scheme: https (index 7 -> 0x87)
    // :path: <path> (literal with index 4: 0x04, length, string)
    // :authority: <authority> (literal with index 1: 0x01, length, string)
    // content-type: application/json (literal with index 31: 0x0f, 0x10, 16, string)
    std::vector<std::uint8_t> hpack;
    hpack.push_back(0x83); // :method: POST
    hpack.push_back(0x87); // :scheme: https

    // :path
    hpack.push_back(0x04);
    hpack.push_back(static_cast<std::uint8_t>(path.size()));
    hpack.insert(hpack.end(), path.begin(), path.end());

    // :authority
    hpack.push_back(0x01);
    hpack.push_back(static_cast<std::uint8_t>(authority.size()));
    hpack.insert(hpack.end(), authority.begin(), authority.end());

    // content-type: application/json
    static const std::string_view kJsonType = "application/json";
    hpack.push_back(0x0f);
    hpack.push_back(0x10);
    hpack.push_back(static_cast<std::uint8_t>(kJsonType.size()));
    hpack.insert(hpack.end(), kJsonType.begin(), kJsonType.end());

    // Wrap in HEADERS frame (Type 0x01, Flags 0x04 END_HEADERS)
    std::vector<std::uint8_t> out;
    const std::uint32_t len = static_cast<std::uint32_t>(hpack.size());
    out.reserve(kHttp2HeaderLen + len);
    write24(out, len);
    out.push_back(0x01); // Type: HEADERS
    out.push_back(0x04); // Flags: END_HEADERS
    write32(out, streamId);
    out.insert(out.end(), hpack.begin(), hpack.end());
    return out;
}

std::vector<std::uint8_t> buildHttp2DataFrame(std::uint32_t streamId,
                                              std::span<const std::uint8_t> data,
                                              bool endStream) {
    std::vector<std::uint8_t> out;
    const std::uint32_t len = static_cast<std::uint32_t>(data.size());
    out.reserve(kHttp2HeaderLen + len);
    write24(out, len);
    out.push_back(0x00); // Type: DATA
    out.push_back(endStream ? 0x01 : 0x00); // Flags: END_STREAM
    write32(out, streamId);
    out.insert(out.end(), data.begin(), data.end());
    return out;
}

bool Http2FrameDecoder::append(std::span<const std::uint8_t> bytes) {
    if (buffer_.size() + bytes.size() > kMaxFrameSize + kHttp2HeaderLen)
        return false;
    buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
    return true;
}

std::optional<Http2Frame> Http2FrameDecoder::take() {
    if (buffer_.size() < kHttp2HeaderLen)
        return std::nullopt;

    const std::uint32_t len = read24(buffer_.data());
    if (len > kMaxFrameSize) {
        buffer_.clear();
        return std::nullopt;
    }

    if (buffer_.size() < kHttp2HeaderLen + len)
        return std::nullopt; // Wait for full frame

    Http2Frame frame;
    frame.length = len;
    frame.type = buffer_[3];
    frame.flags = buffer_[4];
    frame.streamId = read32(buffer_.data() + 5);
    frame.payload.assign(buffer_.begin() + kHttp2HeaderLen,
                         buffer_.begin() + kHttp2HeaderLen + len);

    buffer_.erase(buffer_.begin(), buffer_.begin() + kHttp2HeaderLen + len);
    return frame;
}

} // namespace artemis::tailscale
