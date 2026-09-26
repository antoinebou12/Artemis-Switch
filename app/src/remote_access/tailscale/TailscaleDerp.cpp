#include "TailscaleDerp.hpp"

#include "TailscaleBox.hpp"
#include "TailscaleRandom.hpp"

extern "C" {
#include <monocypher.h>
}

#include <algorithm>
#include <cctype>
#include <cstring>
#include <random>

#if defined(__SWITCH__)
extern "C" {
void tailscale_internal_crypto_wipe(void*, size_t);
void tailscale_internal_crypto_x25519(uint8_t[32], const uint8_t[32], const uint8_t[32]);
void tailscale_internal_crypto_aead_lock(uint8_t*, uint8_t[16], const uint8_t[32],
                                         const uint8_t[24], const uint8_t*, size_t,
                                         const uint8_t*, size_t);
int tailscale_internal_crypto_aead_unlock(uint8_t*, const uint8_t[16], const uint8_t[32],
                                          const uint8_t[24], const uint8_t*, size_t,
                                          const uint8_t*, size_t);
}
#define TS_DERP_WIPE tailscale_internal_crypto_wipe
#define TS_DERP_X25519 tailscale_internal_crypto_x25519
#define TS_DERP_LOCK tailscale_internal_crypto_aead_lock
#define TS_DERP_UNLOCK tailscale_internal_crypto_aead_unlock
#else
#define TS_DERP_WIPE crypto_wipe
#define TS_DERP_X25519 crypto_x25519
#define TS_DERP_LOCK crypto_aead_lock
#define TS_DERP_UNLOCK crypto_aead_unlock
#endif

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
            return std::nullopt;
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

bool DerpCrypto::seal(std::span<const std::uint8_t> plainText,
                      const Key32& myPrivate,
                      const Key32& theirPublic,
                      std::span<const std::uint8_t, kDerpNonceLen> nonce,
                      std::vector<std::uint8_t>& cipherTextOut,
                      std::string* error) {
    static_assert(kDerpNonceLen == kBoxNonceLen,
                  "DERP and NaCl box nonces are both 24 bytes");
    static_assert(kDerpKeyLen == 32, "DERP keys are 32-byte Curve25519 keys");
    std::span<const std::uint8_t, kBoxNonceLen> boxNonce(nonce.data(),
                                                        kBoxNonceLen);
    return boxSeal(plainText, myPrivate, theirPublic, boxNonce, cipherTextOut,
                   error);
}

bool DerpCrypto::open(std::span<const std::uint8_t> cipherText,
                      const Key32& myPrivate,
                      const Key32& theirPublic,
                      std::span<const std::uint8_t, kDerpNonceLen> nonce,
                      std::vector<std::uint8_t>& plainTextOut,
                      std::string* error) {
    if (cipherText.size() < kDerpTagLen) {
        if (error) *error = "DERP crypto: ciphertext shorter than auth tag";
        return false;
    }
    std::span<const std::uint8_t, kBoxNonceLen> boxNonce(nonce.data(),
                                                        kBoxNonceLen);
    return boxOpen(cipherText, myPrivate, theirPublic, boxNonce, plainTextOut,
                   error);
}

DerpSession::DerpSession(std::unique_ptr<ITransport> transport,
                         Key32 clientPrivate,
                         Key32 clientPublic,
                         std::shared_ptr<IDerpCrypto> crypto)
    : transport_(std::move(transport)),
      clientPrivate_(clientPrivate),
      clientPublic_(clientPublic),
      crypto_(std::move(crypto)) {
    if (!crypto_)
        crypto_ = std::make_shared<DerpCrypto>();
}

bool DerpSession::connect(std::string* error) {
    if (error) error->clear();
    if (!transport_) {
        if (error) *error = "DERP transport is not open";
        return false;
    }
    const bool keyMissing =
        std::all_of(clientPrivate_.begin(), clientPrivate_.end(),
                    [](std::uint8_t b) { return b == 0; });
    if (keyMissing) {
        if (error)
            *error = "DERP relay requires valid client node key; refusing to open";
        return false;
    }

    // Step 1: Wait for ServerKey greeting
    auto greeting = recvFrame(error);
    if (!greeting) {
        if (error && error->empty())
            *error = "DERP connect: failed to receive server greeting";
        return false;
    }
    if (greeting->type != DerpFrameType::ServerKey ||
        greeting->payload.size() < kDerpMagic.size() + kDerpKeyLen) {
        if (error)
            *error = "DERP connect: invalid ServerKey greeting";
        return false;
    }
    if (std::memcmp(greeting->payload.data(), kDerpMagic.data(),
                    kDerpMagic.size()) != 0) {
        if (error)
            *error = "DERP connect: invalid magic in ServerKey greeting";
        return false;
    }
    std::copy_n(greeting->payload.begin() + kDerpMagic.size(), kDerpKeyLen,
                serverKey_.begin());

    // Step 2: Send ClientInfo frame. The nonce must be unique per handshake;
    // a fixed nonce across reconnects would leak the keystream.
    std::array<std::uint8_t, kDerpNonceLen> clientNonce{};
    secureRandomBytes(clientNonce);

    const std::string clientInfoJson = R"({"version":2})";
    std::vector<std::uint8_t> sealedClientInfo;
    if (!crypto_->seal(
            std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(clientInfoJson.data()),
                clientInfoJson.size()),
            clientPrivate_, serverKey_, clientNonce, sealedClientInfo, error)) {
        return false;
    }

    std::vector<std::uint8_t> clientPayload;
    clientPayload.reserve(kDerpKeyLen + kDerpNonceLen + sealedClientInfo.size());
    clientPayload.insert(clientPayload.end(), clientPublic_.begin(), clientPublic_.end());
    clientPayload.insert(clientPayload.end(), clientNonce.begin(), clientNonce.end());
    clientPayload.insert(clientPayload.end(), sealedClientInfo.begin(), sealedClientInfo.end());

    if (!writeRaw(DerpFrameType::ClientInfo, clientPayload, error))
        return false;

    // Step 3: Receive ServerInfo frame
    auto serverInfo = recvFrame(error);
    if (!serverInfo) {
        if (error && error->empty())
            *error = "DERP connect: failed to receive ServerInfo";
        return false;
    }
    if (serverInfo->type != DerpFrameType::ServerInfo ||
        serverInfo->payload.size() < kDerpNonceLen + kDerpTagLen) {
        if (error)
            *error = "DERP connect: invalid ServerInfo frame";
        return false;
    }

    std::array<std::uint8_t, kDerpNonceLen> serverNonce{};
    std::copy_n(serverInfo->payload.begin(), kDerpNonceLen, serverNonce.begin());

    std::span<const std::uint8_t> serverCiphertext(
        serverInfo->payload.data() + kDerpNonceLen,
        serverInfo->payload.size() - kDerpNonceLen);
    std::vector<std::uint8_t> decryptedInfo;

    if (!crypto_->open(serverCiphertext, clientPrivate_, serverKey_,
                       serverNonce, decryptedInfo, error)) {
        return false;
    }

    connected_ = true;
    return true;
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
    connected_ = false;
    if (transport_) transport_->close();
}

void DerpSession::interrupt() noexcept {
    // transport_ is created with the session and not replaced by close(), so
    // it stays valid while the reader thread is still running.
    if (transport_) transport_->interrupt();
}

bool DerpSession::isConnected() const noexcept {
    return connected_;
}

Key32 DerpSession::serverKey() const noexcept {
    return serverKey_;
}

std::string buildDerpUpgradeRequest(std::string_view host) {
    std::string request = "GET /derp HTTP/1.1\r\nHost: ";
    request.append(host.data(), host.size());
    request.append("\r\nUpgrade: DERP\r\nConnection: Upgrade\r\n"
                   "User-Agent: artemis-switch\r\n\r\n");
    return request;
}

bool validateDerpUpgradeResponse(std::string_view header, std::string* error) {
    if (header.size() < 16 || header.substr(header.size() - 4) != "\r\n\r\n") {
        if (error) *error = "DERP upgrade response is incomplete";
        return false;
    }
    const auto statusEnd = header.find("\r\n");
    if (statusEnd == std::string_view::npos) {
        if (error) *error = "DERP upgrade response has no status line";
        return false;
    }
    const std::string_view status = header.substr(0, statusEnd);
    const bool http11 = status.starts_with("HTTP/1.1 ");
    const bool http10 = status.starts_with("HTTP/1.0 ");
    const bool is101 = status.size() >= 12 && status.substr(9, 3) == "101";
    if (!(http11 || http10) || !is101) {
        if (error)
            *error = "DERP upgrade rejected: " + std::string(status);
        return false;
    }
    // Scan header lines for an Upgrade token naming DERP. Token matching is
    // case-insensitive and tolerates comma-separated extras.
    std::string_view rest = header.substr(statusEnd + 2);
    while (!rest.empty()) {
        const auto lineEnd = rest.find("\r\n");
        const std::string_view line =
            lineEnd == std::string_view::npos ? rest : rest.substr(0, lineEnd);
        const auto colon = line.find(':');
        if (colon != std::string_view::npos) {
            std::string name(line.substr(0, colon));
            std::transform(name.begin(), name.end(), name.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (name == "upgrade") {
                std::string value(line.substr(colon + 1));
                std::transform(value.begin(), value.end(), value.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                if (value.find("derp") != std::string::npos)
                    return true;
            }
        }
        if (lineEnd == std::string_view::npos)
            break;
        rest.remove_prefix(lineEnd + 2);
    }
    if (error) *error = "DERP upgrade response lacks an Upgrade: DERP header";
    return false;
}

} // namespace artemis::tailscale
