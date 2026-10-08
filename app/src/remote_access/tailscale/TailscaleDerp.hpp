#pragma once

#include "TailscaleTransport.hpp"
#include "TailscaleTypes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace artemis::tailscale {

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
constexpr std::size_t kDerpTagLen = 16;
constexpr std::size_t kDerpMaxPacketSize = 64 * 1024;

// The 8-byte magic sent inside the FrameServerKey greeting ("DERP" + U+1F511).
constexpr std::array<std::uint8_t, 8> kDerpMagic = {
    0x44, 0x45, 0x52, 0x50, 0xF0, 0x9F, 0x94, 0x91};

struct DerpFrame {
    DerpFrameType type = DerpFrameType::KeepAlive;
    std::vector<std::uint8_t> payload;
};

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

// NaCl crypto_box (XSalsa20-Poly1305) for the ClientInfo/ServerInfo
// handshake and, through DerpCrypto, the relay framing auth envelope.
class IDerpCrypto {
public:
    virtual ~IDerpCrypto() = default;
    virtual bool seal(std::span<const std::uint8_t> plainText,
                      const Key32& myPrivate,
                      const Key32& theirPublic,
                      std::span<const std::uint8_t, kDerpNonceLen> nonce,
                      std::vector<std::uint8_t>& cipherTextOut,
                      std::string* error) = 0;
    virtual bool open(std::span<const std::uint8_t> cipherText,
                      const Key32& myPrivate,
                      const Key32& theirPublic,
                      std::span<const std::uint8_t, kDerpNonceLen> nonce,
                      std::vector<std::uint8_t>& plainTextOut,
                      std::string* error) = 0;
};

class DerpCrypto final : public IDerpCrypto {
public:
    bool seal(std::span<const std::uint8_t> plainText,
              const Key32& myPrivate,
              const Key32& theirPublic,
              std::span<const std::uint8_t, kDerpNonceLen> nonce,
              std::vector<std::uint8_t>& cipherTextOut,
              std::string* error) override;
    bool open(std::span<const std::uint8_t> cipherText,
              const Key32& myPrivate,
              const Key32& theirPublic,
              std::span<const std::uint8_t, kDerpNonceLen> nonce,
              std::vector<std::uint8_t>& plainTextOut,
              std::string* error) override;
};

// Builds the plain-HTTP upgrade request that turns a connected (usually TLS)
// byte stream into a DERP frame stream: `GET /derp` with `Upgrade: DERP`.
// The caller writes the result, reads the response header through the first
// CRLF CRLF, and validates it with validateDerpUpgradeResponse before handing
// the transport to DerpSession.
std::string buildDerpUpgradeRequest(std::string_view host);

// Validates the complete HTTP response header (through CRLF CRLF) to a DERP
// upgrade request: status 101 plus an Upgrade token naming DERP
// (case-insensitive, tolerant of extra tokens for headscale compatibility).
// No response body is permitted before DERP framing begins.
bool validateDerpUpgradeResponse(std::string_view header,
                                 std::string* error = nullptr);

class DerpSession {
public:
    explicit DerpSession(std::unique_ptr<ITransport> transport,
                         Key32 clientPrivate = {},
                         Key32 clientPublic = {},
                         std::shared_ptr<IDerpCrypto> crypto = nullptr);

    bool connect(std::string* error);

    bool sendPacket(std::span<const std::uint8_t> destKey,
                    std::span<const std::uint8_t> packet, std::string* error);
    bool sendPing(std::uint64_t token, std::string* error);
    std::optional<DerpFrame> recvFrame(std::string* error);

    bool writeRaw(DerpFrameType type, std::span<const std::uint8_t> payload,
                  std::string* error);
    void close() noexcept;
    // Unblocks a reader thread waiting in the transport (see
    // ITransport::interrupt). The reader's owner still calls close().
    void interrupt() noexcept;

    [[nodiscard]] bool isConnected() const noexcept;
    [[nodiscard]] Key32 serverKey() const noexcept;

private:
    std::unique_ptr<ITransport> transport_;
    DerpCodec codec_;
    Key32 clientPrivate_{};
    Key32 clientPublic_{};
    Key32 serverKey_{};
    std::shared_ptr<IDerpCrypto> crypto_;
    bool connected_ = false;
};

} // namespace artemis::tailscale
