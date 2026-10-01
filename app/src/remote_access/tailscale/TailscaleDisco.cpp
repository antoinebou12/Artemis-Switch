#include "TailscaleDisco.hpp"

#include "TailscaleBox.hpp"

#include <algorithm>
#include <charconv>
#include <cstring>

namespace artemis::tailscale {
namespace {

void putU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value >> 8U));
    out.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

void putU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value >> 24U));
    out.push_back(static_cast<std::uint8_t>(value >> 16U));
    out.push_back(static_cast<std::uint8_t>(value >> 8U));
    out.push_back(static_cast<std::uint8_t>(value & 0xffU));
}

std::uint16_t getU16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>((p[0] << 8U) | p[1]);
}

std::uint32_t getU32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24U) |
           (static_cast<std::uint32_t>(p[1]) << 16U) |
           (static_cast<std::uint32_t>(p[2]) << 8U) | p[3];
}

// Disco carries addresses as 16-byte IPv6, with IPv4 as ::ffff:a.b.c.d.
constexpr std::size_t kAddrPortLen = 18;

void putAddrPort(std::vector<std::uint8_t>& out, const IPv4Endpoint& ep) {
    for (int i = 0; i < 10; ++i)
        out.push_back(0);
    out.push_back(0xff);
    out.push_back(0xff);
    putU32(out, ep.address);
    putU16(out, ep.port);
}

std::optional<IPv4Endpoint> getAddrPort(const std::uint8_t* p) {
    for (int i = 0; i < 10; ++i)
        if (p[i] != 0)
            return std::nullopt;
    if (p[10] != 0xff || p[11] != 0xff)
        return std::nullopt;
    IPv4Endpoint ep{getU32(p + 12), getU16(p + 16)};
    if (!ep.valid())
        return std::nullopt;
    return ep;
}

constexpr std::size_t kStunHeaderLen = 20;
constexpr std::uint16_t kStunBindingRequest = 0x0001;
constexpr std::uint16_t kStunBindingSuccess = 0x0101;
constexpr std::uint16_t kStunAttrMappedAddress = 0x0001;
constexpr std::uint16_t kStunAttrXorMappedAddress = 0x0020;

} // namespace

std::string IPv4Endpoint::toString() const {
    return std::to_string((address >> 24U) & 0xffU) + "." +
           std::to_string((address >> 16U) & 0xffU) + "." +
           std::to_string((address >> 8U) & 0xffU) + "." +
           std::to_string(address & 0xffU) + ":" + std::to_string(port);
}

std::optional<IPv4Endpoint> IPv4Endpoint::parse(std::string_view text) {
    const auto colon = text.rfind(':');
    if (colon == std::string_view::npos)
        return std::nullopt;
    const auto host = text.substr(0, colon);
    const auto portText = text.substr(colon + 1);
    unsigned port = 0;
    const auto portResult = std::from_chars(
        portText.data(), portText.data() + portText.size(), port);
    if (portText.empty() || portResult.ec != std::errc{} ||
        portResult.ptr != portText.data() + portText.size() || port == 0 ||
        port > 65535)
        return std::nullopt;
    std::uint32_t value = 0;
    std::size_t begin = 0;
    int parts = 0;
    while (begin <= host.size()) {
        const auto dot = host.find('.', begin);
        const auto piece = host.substr(
            begin, dot == std::string_view::npos ? std::string_view::npos
                                                 : dot - begin);
        unsigned octet = 0;
        const auto result =
            std::from_chars(piece.data(), piece.data() + piece.size(), octet);
        if (piece.empty() || piece.size() > 3 || result.ec != std::errc{} ||
            result.ptr != piece.data() + piece.size() || octet > 255)
            return std::nullopt;
        value = (value << 8U) | octet;
        ++parts;
        if (dot == std::string_view::npos)
            break;
        begin = dot + 1;
    }
    if (parts != 4)
        return std::nullopt;
    IPv4Endpoint ep{value, static_cast<std::uint16_t>(port)};
    if (!ep.valid())
        return std::nullopt;
    return ep;
}

namespace disco {

bool looksLikeDisco(std::span<const std::uint8_t> packet) noexcept {
    return packet.size() >= kHeaderLen &&
           std::equal(kMagic.begin(), kMagic.end(), packet.begin());
}

std::optional<Key32> senderKey(std::span<const std::uint8_t> packet) {
    if (!looksLikeDisco(packet))
        return std::nullopt;
    Key32 key{};
    std::memcpy(key.data(), packet.data() + kMagic.size(), key.size());
    return key;
}

std::vector<std::uint8_t> encodePlaintext(const Message& message) {
    std::vector<std::uint8_t> out;
    out.push_back(static_cast<std::uint8_t>(message.type));
    out.push_back(0); // version
    switch (message.type) {
    case MessageType::Ping:
        out.insert(out.end(), message.txid.begin(), message.txid.end());
        out.insert(out.end(), message.nodeKey.begin(), message.nodeKey.end());
        break;
    case MessageType::Pong:
        out.insert(out.end(), message.txid.begin(), message.txid.end());
        putAddrPort(out, message.source);
        break;
    case MessageType::CallMeMaybe:
        for (const auto& ep : message.endpoints)
            putAddrPort(out, ep);
        break;
    }
    return out;
}

std::optional<Message> decodePlaintext(std::span<const std::uint8_t> plaintext) {
    if (plaintext.size() < 2)
        return std::nullopt;
    Message message;
    const auto body = plaintext.subspan(2);
    switch (plaintext[0]) {
    case static_cast<std::uint8_t>(MessageType::Ping):
        message.type = MessageType::Ping;
        if (body.size() < message.txid.size())
            return std::nullopt;
        std::memcpy(message.txid.data(), body.data(), message.txid.size());
        if (body.size() >= message.txid.size() + 32) {
            std::memcpy(message.nodeKey.data(),
                        body.data() + message.txid.size(), 32);
            message.hasNodeKey = true;
        }
        return message;
    case static_cast<std::uint8_t>(MessageType::Pong): {
        message.type = MessageType::Pong;
        if (body.size() < message.txid.size() + kAddrPortLen)
            return std::nullopt;
        std::memcpy(message.txid.data(), body.data(), message.txid.size());
        // IPv6 sources are valid pongs; only the IPv4 form is reported.
        if (auto ep = getAddrPort(body.data() + message.txid.size()))
            message.source = *ep;
        return message;
    }
    case static_cast<std::uint8_t>(MessageType::CallMeMaybe):
        message.type = MessageType::CallMeMaybe;
        if (body.size() % kAddrPortLen != 0)
            return std::nullopt;
        for (std::size_t off = 0; off < body.size(); off += kAddrPortLen) {
            if (message.endpoints.size() >= DirectPath::kMaxCandidates)
                break;
            if (auto ep = getAddrPort(body.data() + off))
                message.endpoints.push_back(*ep);
        }
        return message;
    default:
        return std::nullopt; // unknown types are ignored by design
    }
}

std::vector<std::uint8_t> seal(const Message& message, const Key32& myPrivate,
                               const Key32& myPublic, const Key32& theirPublic,
                               std::span<const std::uint8_t, kNonceLen> nonce) {
    const auto plaintext = encodePlaintext(message);
    std::vector<std::uint8_t> boxed;
    if (!boxSeal(plaintext, myPrivate, theirPublic, nonce, boxed))
        return {};
    std::vector<std::uint8_t> out;
    out.reserve(kHeaderLen + boxed.size());
    out.insert(out.end(), kMagic.begin(), kMagic.end());
    out.insert(out.end(), myPublic.begin(), myPublic.end());
    out.insert(out.end(), nonce.begin(), nonce.end());
    out.insert(out.end(), boxed.begin(), boxed.end());
    return out;
}

std::optional<Opened> open(std::span<const std::uint8_t> packet,
                           const Key32& myPrivate) {
    const auto sender = senderKey(packet);
    if (!sender)
        return std::nullopt;
    const std::span<const std::uint8_t, kNonceLen> nonce(
        packet.data() + kMagic.size() + 32, kNonceLen);
    std::vector<std::uint8_t> plaintext;
    if (!boxOpen(packet.subspan(kHeaderLen), myPrivate, *sender, nonce,
                 plaintext))
        return std::nullopt;
    auto message = decodePlaintext(plaintext);
    if (!message)
        return std::nullopt;
    return Opened{*sender, std::move(*message)};
}

} // namespace disco

namespace stun {

std::vector<std::uint8_t> bindingRequest(const TxId& txid) {
    std::vector<std::uint8_t> out;
    out.reserve(kStunHeaderLen);
    putU16(out, kStunBindingRequest);
    putU16(out, 0); // no attributes
    putU32(out, kMagicCookie);
    out.insert(out.end(), txid.begin(), txid.end());
    return out;
}

bool looksLikeStun(std::span<const std::uint8_t> packet) noexcept {
    return packet.size() >= kStunHeaderLen && (packet[0] & 0xc0U) == 0 &&
           getU32(packet.data() + 4) == kMagicCookie;
}

std::optional<IPv4Endpoint> parseBindingResponse(
    std::span<const std::uint8_t> packet, const TxId& txid) {
    if (!looksLikeStun(packet) || getU16(packet.data()) != kStunBindingSuccess)
        return std::nullopt;
    if (!std::equal(txid.begin(), txid.end(), packet.begin() + 8))
        return std::nullopt;
    const std::size_t length = getU16(packet.data() + 2);
    if (kStunHeaderLen + length > packet.size())
        return std::nullopt;
    std::optional<IPv4Endpoint> mapped;
    std::size_t off = kStunHeaderLen;
    const std::size_t end = kStunHeaderLen + length;
    while (off + 4 <= end) {
        const auto type = getU16(packet.data() + off);
        const std::size_t attrLen = getU16(packet.data() + off + 2);
        const std::size_t value = off + 4;
        if (value + attrLen > end)
            break;
        // value: reserved(1) family(1) port(2) address(4) for IPv4.
        if ((type == kStunAttrXorMappedAddress ||
             type == kStunAttrMappedAddress) &&
            attrLen >= 8 && packet[value + 1] == 0x01) {
            std::uint16_t port = getU16(packet.data() + value + 2);
            std::uint32_t address = getU32(packet.data() + value + 4);
            if (type == kStunAttrXorMappedAddress) {
                port ^= static_cast<std::uint16_t>(kMagicCookie >> 16U);
                address ^= kMagicCookie;
                IPv4Endpoint ep{address, port};
                if (ep.valid())
                    return ep; // XOR form wins over the legacy one
            } else if (!mapped) {
                IPv4Endpoint ep{address, port};
                if (ep.valid())
                    mapped = ep;
            }
        }
        off = value + ((attrLen + 3U) & ~std::size_t{3});
    }
    return mapped;
}

} // namespace stun

void DirectPath::addCandidate(const IPv4Endpoint& endpoint) {
    if (!endpoint.valid() || isCandidate(endpoint))
        return;
    if (candidates_.size() >= kMaxCandidates)
        candidates_.erase(candidates_.begin()); // keep the newest
    candidates_.push_back({endpoint});
}

bool DirectPath::isCandidate(const IPv4Endpoint& endpoint) const {
    return std::any_of(candidates_.begin(), candidates_.end(),
                       [&](const Candidate& c) { return c.endpoint == endpoint; });
}

std::vector<IPv4Endpoint> DirectPath::due(Clock::time_point now) {
    pending_.erase(std::remove_if(pending_.begin(), pending_.end(),
                                  [&](const Pending& p) {
                                      return now - p.sent > kPingTimeout;
                                  }),
                   pending_.end());
    std::vector<IPv4Endpoint> out;
    for (const auto& candidate : candidates_) {
        if (!candidate.pinged || now - candidate.lastPing >= kHeartbeat)
            out.push_back(candidate.endpoint);
    }
    return out;
}

void DirectPath::notePingSent(const TxId& txid, const IPv4Endpoint& endpoint,
                              Clock::time_point now) {
    for (auto& candidate : candidates_) {
        if (candidate.endpoint == endpoint) {
            candidate.lastPing = now;
            candidate.pinged = true;
        }
    }
    if (pending_.size() >= kMaxCandidates * 4)
        pending_.erase(pending_.begin());
    pending_.push_back({txid, endpoint, now});
}

std::optional<int> DirectPath::notePong(const TxId& txid,
                                        const IPv4Endpoint& from,
                                        Clock::time_point now) {
    const auto found =
        std::find_if(pending_.begin(), pending_.end(),
                     [&](const Pending& p) { return p.txid == txid; });
    if (found == pending_.end())
        return std::nullopt;
    // A pong must come back from the address that was pinged; anything else
    // is a stale or forged reply and must not steer traffic.
    if (!(found->endpoint == from) || now - found->sent > kPingTimeout) {
        pending_.erase(found);
        return std::nullopt;
    }
    const int rtt = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - found->sent)
            .count());
    pending_.erase(found);
    const bool bestValid = best(now).has_value();
    // Switch only to a clearly better path, so two similar paths do not flap.
    if (!bestValid || from == best_ || rtt * 10 < bestRttMs_ * 9) {
        best_ = from;
        bestRttMs_ = rtt;
        bestPong_ = now;
    }
    return rtt;
}

std::optional<IPv4Endpoint> DirectPath::best(Clock::time_point now) const {
    if (!best_.valid() || now - bestPong_ > kTrustDuration)
        return std::nullopt;
    return best_;
}

void DirectPath::reset() {
    candidates_.clear();
    pending_.clear();
    best_ = {};
    bestPong_ = {};
    bestRttMs_ = -1;
}

} // namespace artemis::tailscale
