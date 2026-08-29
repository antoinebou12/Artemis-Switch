#include "TailscaleDerp.hpp"

#include <cstring>

namespace artemis::tailscale {

namespace {
void writeBigEndian32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value >> 24U));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    out.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

std::optional<std::uint32_t> readBigEndian32(std::span<const std::uint8_t> b) {
    if (b.size() < 4)
        return std::nullopt;
    return (static_cast<std::uint32_t>(b[0]) << 24U) |
           (static_cast<std::uint32_t>(b[1]) << 16U) |
           (static_cast<std::uint32_t>(b[2]) << 8U) |
           static_cast<std::uint32_t>(b[3]);
}
} // namespace

std::vector<std::uint8_t>
DerpCodec::encode(DerpFrameType type, std::span<const std::uint8_t> payload) {
    std::vector<std::uint8_t> out;
    out.reserve(kDerpFrameHeaderLen + payload.size());
    out.push_back(static_cast<std::uint8_t>(type));
    writeBigEndian32(out, static_cast<std::uint32_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

std::optional<DerpFrame> DerpCodec::parse(std::span<const std::uint8_t> bytes,
                                          std::string* error) {
    if (bytes.size() < kDerpFrameHeaderLen)
        return std::nullopt;
    const auto length = readBigEndian32(
        bytes.subspan(1, 4));
    if (!length || *length > kMaxFramePayload ||
        bytes.size() < kDerpFrameHeaderLen + *length) {
        if (error && *length > kMaxFramePayload) *error = "DERP frame oversized";
        return std::nullopt;
    }
    DerpFrame frame;
    frame.type = static_cast<DerpFrameType>(bytes[0]);
    const auto payloadSize = *length;
    frame.payload.assign(bytes.begin() + kDerpFrameHeaderLen,
                         bytes.begin() + kDerpFrameHeaderLen + payloadSize);
    return frame;
}

bool DerpCodec::append(std::span<const std::uint8_t> bytes, std::string* error) {
    const std::size_t limit = kMaxFramePayload + kDerpFrameHeaderLen;
    if (bytes.size() > limit || buffer_.size() > limit - bytes.size()) {
        if (error) *error = "DERP receive buffer limit exceeded";
        buffer_.clear();
        return false;
    }
    buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
    return true;
}

std::optional<DerpFrame> DerpCodec::take(std::string* error) {
    while (buffer_.size() >= kDerpFrameHeaderLen) {
        const auto length = readBigEndian32(
            std::span<const std::uint8_t>(buffer_).subspan(1, 4));
        if (!length || *length > kMaxFramePayload) {
            if (error) *error = "DERP frame oversized";
            buffer_.clear();
            return std::nullopt;
        }
        if (buffer_.size() < kDerpFrameHeaderLen + *length)
            return std::nullopt;  // wait for the rest of the frame
        auto frame = parse(
            std::span<const std::uint8_t>(buffer_)
                .first(kDerpFrameHeaderLen + *length),
            error);
        if (!frame)
            return std::nullopt;
        buffer_.erase(buffer_.begin(),
                      buffer_.begin() + kDerpFrameHeaderLen + *length);
        return frame;
    }
    return std::nullopt;
}

bool DerpSession::connect(std::string* error) {
    // The naclbox ClientInfo/ServerInfo relay handshake is not implemented, so
    // a session must never be treated as usable. Fail closed with a reason.
    if (error)
        *error =
            "DERP relay handshake is not implemented; refusing to open";
    return false;
}

bool DerpSession::sendPacket(std::span<const std::uint8_t> destKey,
                             std::span<const std::uint8_t> packet,
                             std::string* error) {
    if (!transport_ || destKey.size() != kDerpKeyLen || packet.empty()) {
        if (error) *error = "invalid DERP SendPacket";
        return false;
    }
    if (packet.size() > kDerpMaxPacketSize) {
        if (error) *error = "DERP packet oversized";
        return false;
    }
    std::vector<std::uint8_t> payload(destKey.begin(), destKey.end());
    payload.insert(payload.end(), packet.begin(), packet.end());
    return writeRaw(DerpFrameType::SendPacket, payload, error);
}

bool DerpSession::sendPing(std::uint64_t token, std::string* error) {
    std::array<std::uint8_t, 8> payload{};
    for (std::size_t i = 0; i < 8; ++i)
        payload[i] = static_cast<std::uint8_t>(token >> (i * 8));
    return writeRaw(DerpFrameType::Ping, payload, error);
}

std::optional<DerpFrame> DerpSession::recvFrame(std::string* error) {
    if (!transport_) {
        if (error) *error = "DERP transport is not open";
        return std::nullopt;
    }
    if (auto frame = codec_.take(error))
        return frame;
    std::array<std::uint8_t, 2048> buffer{};
    for (;;) {
        const int received = transport_->read(buffer.data(), buffer.size(), error);
        if (received < 0) return std::nullopt;
        if (received == 0) {
            if (error) *error = "DERP connection closed";
            return std::nullopt;
        }
        if (!codec_.append(
                std::span<const std::uint8_t>(buffer.data(),
                                              static_cast<std::size_t>(received)),
                error))
            return std::nullopt;
        if (auto frame = codec_.take(error))
            return frame;
    }
}

bool DerpSession::writeRaw(DerpFrameType type,
                           std::span<const std::uint8_t> payload,
                           std::string* error) {
    if (!transport_) {
        if (error) *error = "DERP transport is not open";
        return false;
    }
    auto encoded = DerpCodec::encode(type, payload);
    if (!transport_->write(encoded, error)) {
        if (error && error->empty()) *error = "DERP write failed";
        return false;
    }
    return true;
}

void DerpSession::close() noexcept {
    if (transport_) transport_->close();
}

} // namespace artemis::tailscale