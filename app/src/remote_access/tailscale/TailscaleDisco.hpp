#pragma once

// Tailscale direct-path primitives, kept free of sockets and platform headers
// so they are unit tested on the host:
//  * disco: the encrypted ping/pong/call-me-maybe messages peers exchange to
//    find a working UDP path (wire format of tailscale.com/disco),
//  * stun: a minimal RFC 5389 Binding request/response codec used to learn
//    this node's public (NAT-mapped) UDP endpoint from a DERP node,
//  * DirectPath: which peer endpoint to trust for WireGuard, driven purely by
//    pong replies so a dead path falls back to DERP on its own.

#include "TailscaleTypes.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace artemis::tailscale {

// An IPv4 UDP endpoint. The address is kept in host byte order.
struct IPv4Endpoint {
    std::uint32_t address = 0;
    std::uint16_t port = 0;

    bool operator==(const IPv4Endpoint&) const = default;
    [[nodiscard]] bool valid() const noexcept { return address != 0 && port != 0; }
    [[nodiscard]] std::string toString() const;
    // "a.b.c.d:port"; nullopt for anything else (IPv6, names, port 0).
    static std::optional<IPv4Endpoint> parse(std::string_view text);
};

using TxId = std::array<std::uint8_t, 12>;

namespace disco {

// "TS" + U+1F4AC (speech balloon).
inline constexpr std::array<std::uint8_t, 6> kMagic = {0x54, 0x53, 0xf0,
                                                       0x9f, 0x92, 0xac};
inline constexpr std::size_t kNonceLen = 24;
// magic + sender disco public key + nonce
inline constexpr std::size_t kHeaderLen = kMagic.size() + 32 + kNonceLen;
// Address Tailscale reports as the pong source for replies sent over DERP.
inline constexpr std::uint32_t kDerpMagicAddress = 0x7f030328; // 127.3.3.40

enum class MessageType : std::uint8_t {
    Ping = 0x01,
    Pong = 0x02,
    CallMeMaybe = 0x03,
};

struct Message {
    MessageType type = MessageType::Ping;
    TxId txid{};
    // Ping: sender's node key (optional for very old peers).
    Key32 nodeKey{};
    bool hasNodeKey = false;
    // Pong: how the responder saw the pinger.
    IPv4Endpoint source{};
    // CallMeMaybe: endpoints the sender wants to be pinged on.
    std::vector<IPv4Endpoint> endpoints;
};

[[nodiscard]] bool looksLikeDisco(std::span<const std::uint8_t> packet) noexcept;
// Sender disco public key from the cleartext header, or nullopt.
std::optional<Key32> senderKey(std::span<const std::uint8_t> packet);

std::vector<std::uint8_t> encodePlaintext(const Message& message);
std::optional<Message> decodePlaintext(std::span<const std::uint8_t> plaintext);

// Full disco packet: header || crypto_box(plaintext).
std::vector<std::uint8_t> seal(const Message& message, const Key32& myPrivate,
                               const Key32& myPublic, const Key32& theirPublic,
                               std::span<const std::uint8_t, kNonceLen> nonce);

struct Opened {
    Key32 sender{};
    Message message;
};
// Opens a disco packet addressed to `myPrivate`. The caller decides whether
// the sender key belongs to a known peer.
std::optional<Opened> open(std::span<const std::uint8_t> packet,
                           const Key32& myPrivate);

} // namespace disco

namespace stun {

inline constexpr std::uint32_t kMagicCookie = 0x2112A442;
inline constexpr std::uint16_t kDefaultPort = 3478;

// Binding request in the exact form Tailscale's STUN servers accept: they
// drop requests without SOFTWARE "tailnode" and a valid FINGERPRINT.
std::vector<std::uint8_t> bindingRequest(const TxId& txid);
// RFC 5389 FINGERPRINT value: CRC-32 (IEEE) of `bytes` XOR 0x5354554e.
std::uint32_t fingerprint(std::span<const std::uint8_t> bytes);
[[nodiscard]] bool looksLikeStun(std::span<const std::uint8_t> packet) noexcept;
// Mapped address from a Binding success response with this transaction ID.
std::optional<IPv4Endpoint> parseBindingResponse(
    std::span<const std::uint8_t> packet, const TxId& txid);

} // namespace stun

// Path choice for one peer. Candidates come from the netmap and from the
// peer's call-me-maybe; each is pinged on a heartbeat and the endpoint with
// the best recent pong is trusted for a short window. With no fresh pong the
// best endpoint expires and traffic goes back to DERP.
class DirectPath {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto kTrustDuration = std::chrono::milliseconds(6500);
    static constexpr auto kHeartbeat = std::chrono::seconds(3);
    static constexpr auto kPingTimeout = std::chrono::seconds(5);
    static constexpr std::size_t kMaxCandidates = 16;

    void addCandidate(const IPv4Endpoint& endpoint);
    [[nodiscard]] bool isCandidate(const IPv4Endpoint& endpoint) const;
    [[nodiscard]] std::size_t candidateCount() const noexcept {
        return candidates_.size();
    }
    // Candidates whose heartbeat is due. The caller pings each and reports
    // it with notePingSent.
    std::vector<IPv4Endpoint> due(Clock::time_point now);
    void notePingSent(const TxId& txid, const IPv4Endpoint& endpoint,
                      Clock::time_point now);
    // Round-trip time in ms when the pong answers an outstanding ping from
    // the same endpoint; nullopt otherwise (unknown, spoofed or stale).
    std::optional<int> notePong(const TxId& txid, const IPv4Endpoint& from,
                                Clock::time_point now);
    [[nodiscard]] std::optional<IPv4Endpoint> best(Clock::time_point now) const;
    [[nodiscard]] int bestRttMs() const noexcept { return bestRttMs_; }
    void reset();

private:
    struct Candidate {
        IPv4Endpoint endpoint;
        Clock::time_point lastPing{};
        bool pinged = false;
    };
    struct Pending {
        TxId txid{};
        IPv4Endpoint endpoint;
        Clock::time_point sent{};
    };

    std::vector<Candidate> candidates_;
    std::vector<Pending> pending_;
    IPv4Endpoint best_{};
    Clock::time_point bestPong_{};
    int bestRttMs_ = -1;
};

} // namespace artemis::tailscale
