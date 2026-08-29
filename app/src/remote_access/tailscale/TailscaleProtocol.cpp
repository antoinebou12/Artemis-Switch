#include "TailscaleProtocol.hpp"

#include <algorithm>
#include <limits>

namespace artemis::tailscale {
namespace {

constexpr std::uint32_t kStunCookie = 0x2112A442;
constexpr std::array<std::uint8_t, 6> kDiscoMagic{
    0x54, 0x53, 0xf0, 0x9f, 0x92, 0xac};

std::uint16_t read16(std::span<const std::uint8_t> bytes) {
    return static_cast<std::uint16_t>(bytes[0] << 8U | bytes[1]);
}

std::uint32_t read32(std::span<const std::uint8_t> bytes) {
    return static_cast<std::uint32_t>(bytes[0]) << 24U |
           static_cast<std::uint32_t>(bytes[1]) << 16U |
           static_cast<std::uint32_t>(bytes[2]) << 8U |
           static_cast<std::uint32_t>(bytes[3]);
}

void append32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value >> 24U));
    out.push_back(static_cast<std::uint8_t>(value >> 16U));
    out.push_back(static_cast<std::uint8_t>(value >> 8U));
    out.push_back(static_cast<std::uint8_t>(value));
}

} // namespace

bool DerpFrameDecoder::append(std::span<const std::uint8_t> bytes,
                              std::string* error) {
    if (bytes.size() > kMaxPayload + 5 ||
        buffer_.size() > kMaxPayload + 5 - bytes.size()) {
        if (error) *error = "DERP receive buffer limit exceeded";
        reset();
        return false;
    }
    buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
    return true;
}

std::optional<DerpFrame> DerpFrameDecoder::take(std::string* error) {
    if (buffer_.size() < 5)
        return std::nullopt;
    const auto payloadSize = read32(std::span(buffer_).subspan(1, 4));
    if (payloadSize > kMaxPayload) {
        if (error) *error = "DERP frame is oversized";
        reset();
        return std::nullopt;
    }
    const auto frameSize = static_cast<std::size_t>(payloadSize) + 5;
    if (buffer_.size() < frameSize)
        return std::nullopt;
    DerpFrame frame;
    frame.type = buffer_[0];
    frame.payload.assign(buffer_.begin() + 5,
                         buffer_.begin() + static_cast<std::ptrdiff_t>(frameSize));
    buffer_.erase(buffer_.begin(),
                  buffer_.begin() + static_cast<std::ptrdiff_t>(frameSize));
    return frame;
}

void DerpFrameDecoder::reset() { buffer_.clear(); }

std::vector<std::uint8_t> encodeDerpFrame(
    std::uint8_t type, std::span<const std::uint8_t> payload) {
    if (payload.size() > DerpFrameDecoder::kMaxPayload)
        return {};
    std::vector<std::uint8_t> result;
    result.reserve(payload.size() + 5);
    result.push_back(type);
    append32(result, static_cast<std::uint32_t>(payload.size()));
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

std::array<std::uint8_t, 20> makeStunBindingRequest(
    const StunTransaction& transaction) {
    std::array<std::uint8_t, 20> request{};
    request[0] = 0x00;
    request[1] = 0x01;
    request[4] = 0x21;
    request[5] = 0x12;
    request[6] = 0xa4;
    request[7] = 0x42;
    std::copy(transaction.id.begin(), transaction.id.end(), request.begin() + 8);
    return request;
}

std::optional<StunEndpoint> parseStunBindingResponse(
    std::span<const std::uint8_t> packet,
    const StunTransaction& transaction, std::string* error) {
    if (packet.size() < 20 || read16(packet.first(2)) != 0x0101 ||
        read32(packet.subspan(4, 4)) != kStunCookie) {
        if (error) *error = "invalid STUN binding response";
        return std::nullopt;
    }
    if (!std::equal(transaction.id.begin(), transaction.id.end(),
                    packet.begin() + 8)) {
        if (error) *error = "STUN transaction mismatch";
        return std::nullopt;
    }
    const auto messageLength = read16(packet.subspan(2, 2));
    if ((messageLength & 3U) != 0 || messageLength > packet.size() - 20) {
        if (error) *error = "truncated STUN response";
        return std::nullopt;
    }
    std::size_t offset = 20;
    const std::size_t end = 20 + messageLength;
    while (offset + 4 <= end) {
        const auto type = read16(packet.subspan(offset, 2));
        const auto length = read16(packet.subspan(offset + 2, 2));
        offset += 4;
        if (offset + length > end) {
            if (error) *error = "truncated STUN attribute";
            return std::nullopt;
        }
        if (type == 0x0020 && length == 8 && packet[offset + 1] == 0x01) {
            StunEndpoint endpoint;
            endpoint.port = read16(packet.subspan(offset + 2, 2)) ^
                            static_cast<std::uint16_t>(kStunCookie >> 16U);
            const auto address = read32(packet.subspan(offset + 4, 4)) ^
                                 kStunCookie;
            endpoint.address =
                std::to_string((address >> 24U) & 0xffU) + "." +
                std::to_string((address >> 16U) & 0xffU) + "." +
                std::to_string((address >> 8U) & 0xffU) + "." +
                std::to_string(address & 0xffU);
            return endpoint;
        }
        const auto paddedLength =
            (static_cast<std::size_t>(length) + 3U) & ~std::size_t{3U};
        if (paddedLength > end - offset) {
            if (error) *error = "truncated STUN attribute padding";
            return std::nullopt;
        }
        offset += paddedLength;
    }
    if (error) *error = "STUN response has no IPv4 XOR-MAPPED-ADDRESS";
    return std::nullopt;
}

bool looksLikeDiscoWrapper(std::span<const std::uint8_t> packet) {
    return packet.size() >= kDiscoMagic.size() + 32 + 24 &&
           std::equal(kDiscoMagic.begin(), kDiscoMagic.end(), packet.begin());
}

std::optional<DiscoMessage> parseDiscoPayload(
    std::span<const std::uint8_t> payload, std::string* error) {
    if (payload.size() < 14) {
        if (error) *error = "short Disco payload";
        return std::nullopt;
    }
    DiscoMessage message;
    message.type = static_cast<DiscoType>(payload[0]);
    message.version = payload[1];
    if (message.version != 0 ||
        (message.type != DiscoType::Ping &&
         message.type != DiscoType::Pong)) {
        if (error) *error = "unsupported Disco message";
        return std::nullopt;
    }
    std::copy_n(payload.begin() + 2, 12, message.transaction.begin());
    if (message.type == DiscoType::Ping && payload.size() >= 46) {
        std::copy_n(payload.begin() + 14, 32, message.nodeKey.begin());
        message.hasNodeKey = true;
    }
    return message;
}

std::vector<std::uint8_t> encodeDiscoPayload(const DiscoMessage& message) {
    std::vector<std::uint8_t> result;
    result.reserve(46);
    result.push_back(static_cast<std::uint8_t>(message.type));
    result.push_back(message.version);
    result.insert(result.end(), message.transaction.begin(),
                  message.transaction.end());
    if (message.type == DiscoType::Ping && message.hasNodeKey)
        result.insert(result.end(), message.nodeKey.begin(), message.nodeKey.end());
    return result;
}

} // namespace artemis::tailscale
