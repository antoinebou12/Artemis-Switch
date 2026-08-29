#pragma once

#include "TailscaleTransport.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace artemis::tailscale {

// DERP relay wire protocol, matching Tailscale's own implementation
// (derp/derp.go). Frames have NO per-frame magic and are delimited by a fixed
// 5-byte header: [FrameType byte][big-endian uint32 length][payload]. The 8-byte
// "DERP🔑" magic appears only in the FrameServerKey greeting payload. All
// multi-byte numbers are big-endian.
//
// The relay identity handshake (ClientInfo/ServerInfo) uses NaCl crypto_box
// (X25519 + XSalsa20Poly1305) with a 24-byte random nonce and requires a valid
// Tailscale node key, so a DERP session fails closed until that layer exists.

enum class DerpFrameType : std::uint8_t {
    ServerKey = 0x01,       // 8B magic + 32B key
    ClientInfo = 0x02,      // 32B client pub + 24B nonce + naclbox(json)
    ServerInfo = 0x03,      // 24B nonce + naclbox(json)
    SendPacket = 0x04,      // 32B dest pub key + packet bytes
    RecvPacket = 0x05,      // v2: 32B src pub key + packet bytes
    KeepAlive = 0x06,       // no payload
    NotePreferred = 0x07,   // 1 byte 0x00/0x01
    PeerGone = 0x08,        // 32B pub key + [1 reason byte]
    PeerPresent = 0x09,     // 32B key + optional addr/flags/name
    ForwardPacket = 0x0a,   // 32B src pub + 32B dst pub + packet
    WatchConns = 0x10,      // no payload
    ClosePeer = 0x11,       // 32B target key
    Ping = 0x12,            // 8 byte payload to be echoed back
    Pong = 0x13,            // 8 byte payload (echo of a Ping)
    Health = 0x14,          // text body, empty = healthy
    Restarting = 0x15,      // two big-endian uint32 ms durations
};

constexpr std::size_t kDerpFrameHeaderLen = 5;  // type(1) + length BE u32(4)
constexpr std::size_t kDerpKeyLen = 32;
constexpr std::size_t kDerpNonceLen = 24;
constexpr std::size_t kDerpMaxPacketSize = 64 * 1024;

// The 8-byte magic sent inside the FrameServerKey greeting ("DERP" + U+1F511).
constexpr std::array<std::uint8_t, 8> kDerpMagic = {
    0x44, 0x45, 0x52, 0x50, 0xF0, 0x9F, 0x94, 0x91};

struct DerpFrame {
    DerpFrameType type = DerpFrameType::KeepAlive;
    std::vector<std::uint8_t> payload;
};

// Bounded encoder/decoder for the 5-byte DERP frame container. Encode wraps a
// payload with type + big-endian length; Decode accumulates a byte stream and
// returns complete frames, guarding the receive buffer and per-frame size.
class DerpCodec {
public:
    static constexpr std::size_t kMaxFramePayload = 1 << 20;  // MaxInfoLen

    static std::vector<std::uint8_t> encode(DerpFrameType type,
                                            std::span<const std::uint8_t> payload);
    static std::optional<DerpFrame> parse(std::span<const std::uint8_t> bytes,
                                          std::string* error = nullptr);

    bool append(std::span<const std::uint8_t> bytes, std::string* error = nullptr);
    std::optional<DerpFrame> take(std::string* error = nullptr);

private:
    std::vector<std::uint8_t> buffer_;
};

// A DERP relay session on an injected byte-stream. connect() currently FAILS
// CLOSED because the relay identity handshake (naclbox ClientInfo/ServerInfo)
// is not yet implemented; the session exposes the pure packet-forward framing
// (sendPacket/recvFrame/ping) for already-WireGuard-encrypted Tailscale
// packets on top of an established transport.
class DerpSession {
public:
    explicit DerpSession(std::unique_ptr<ITransport> transport)
        : transport_(std::move(transport)) {}

    // Until the naclbox relay handshake exists this always fails closed so a
    // caller can never treat a half-open relay as usable.
    bool connect(std::string* error);

    bool sendPacket(std::span<const std::uint8_t> destKey,
                    std::span<const std::uint8_t> packet, std::string* error);
    bool sendPing(std::uint64_t token, std::string* error);
    std::optional<DerpFrame> recvFrame(std::string* error);

    bool writeRaw(DerpFrameType type, std::span<const std::uint8_t> payload,
                  std::string* error);
    void close() noexcept;

private:
    std::unique_ptr<ITransport> transport_;
    DerpCodec codec_;
};

} // namespace artemis::tailscale