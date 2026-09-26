#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace artemis::tailscale {

struct Http2Frame {
    std::uint32_t length = 0;
    std::uint8_t type = 0;
    std::uint8_t flags = 0;
    std::uint32_t streamId = 0;
    std::vector<std::uint8_t> payload;
};

// ponytail: HTTP/2 ceiling is single-stream control client; upgrade to full nghttp2 if multi-stream multiplexing required.
std::vector<std::uint8_t> buildHttp2ClientPreface();
std::vector<std::uint8_t> buildHttp2SettingsAck();
std::vector<std::uint8_t> buildHttp2PingAck(std::span<const std::uint8_t> opaqueData);
std::vector<std::uint8_t> buildHttp2PostHeaders(std::uint32_t streamId,
                                                std::string_view authority,
                                                std::string_view path);
std::vector<std::uint8_t> buildHttp2DataFrame(std::uint32_t streamId,
                                              std::span<const std::uint8_t> data,
                                              bool endStream = false);

class Http2FrameDecoder {
public:
    static constexpr std::size_t kMaxFrameSize = 1024 * 1024;

    bool append(std::span<const std::uint8_t> bytes);
    std::optional<Http2Frame> take();

private:
    std::vector<std::uint8_t> buffer_;
};

} // namespace artemis::tailscale
