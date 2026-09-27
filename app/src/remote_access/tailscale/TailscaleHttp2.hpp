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

// Receive windows this client advertises. The HTTP/2 default is 65535 bytes;
// a full netmap alone can exceed that, and the long-poll map stream carries
// every later update, so both windows are raised at connection start and
// replenished with WINDOW_UPDATE as data is consumed.
inline constexpr std::uint32_t kHttp2DefaultWindow = 65535;
inline constexpr std::uint32_t kHttp2ClientStreamWindow = 4U * 1024U * 1024U;
inline constexpr std::uint32_t kHttp2ClientConnectionWindow =
    16U * 1024U * 1024U;

// ponytail: HTTP/2 ceiling is single-stream control client; upgrade to full nghttp2 if multi-stream multiplexing required.
// Connection preface, SETTINGS (INITIAL_WINDOW_SIZE = stream window) and a
// connection-level WINDOW_UPDATE raising the connection window.
std::vector<std::uint8_t> buildHttp2ClientPreface();
// WINDOW_UPDATE frame; streamId 0 targets the connection window.
// increment must be 1..2^31-1.
std::vector<std::uint8_t> buildHttp2WindowUpdate(std::uint32_t streamId,
                                                 std::uint32_t increment);
std::vector<std::uint8_t> buildHttp2SettingsAck();
std::vector<std::uint8_t> buildHttp2PingAck(std::span<const std::uint8_t> opaqueData);
std::vector<std::uint8_t> buildHttp2PostHeaders(std::uint32_t streamId,
                                                std::string_view authority,
                                                std::string_view path);
std::vector<std::uint8_t> buildHttp2DataFrame(std::uint32_t streamId,
                                              std::span<const std::uint8_t> data,
                                              bool endStream = false);

// Receive-side flow-control bookkeeping for one window (a stream or the
// connection). consume() returns the WINDOW_UPDATE increment to send once
// half the window has been used, so the sender never stalls.
class Http2ReceiveWindow {
public:
    explicit Http2ReceiveWindow(std::uint32_t size) : size_(size) {}
    std::optional<std::uint32_t> consume(std::uint32_t flowControlledBytes);
    void reset() noexcept { unacknowledged_ = 0; }

private:
    std::uint32_t size_;
    std::uint64_t unacknowledged_ = 0;
};

class Http2FrameDecoder {
public:
    static constexpr std::size_t kMaxFrameSize = 1024 * 1024;

    bool append(std::span<const std::uint8_t> bytes);
    std::optional<Http2Frame> take();

private:
    std::vector<std::uint8_t> buffer_;
};

} // namespace artemis::tailscale
