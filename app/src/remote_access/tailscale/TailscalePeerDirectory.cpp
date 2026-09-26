#include "TailscalePeerDirectory.hpp"

#include <charconv>
#include <mutex>
#include <unordered_set>

namespace artemis::tailscale {
namespace {

bool parseOctet(std::string_view value) {
    if (value.empty() || value.size() > 3)
        return false;
    unsigned int octet = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(),
                                        octet);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size() &&
           octet <= 255;
}

bool parseIPv4Value(std::string_view ip, std::uint32_t* out) {
    std::size_t begin = 0;
    int components = 0;
    std::uint32_t val = 0;
    while (begin <= ip.size()) {
        const auto dot = ip.find('.', begin);
        const auto end = dot == std::string_view::npos ? ip.size() : dot;
        const auto piece = ip.substr(begin, end - begin);
        if (!parseOctet(piece))
            return false;
        unsigned int octet = 0;
        std::from_chars(piece.data(), piece.data() + piece.size(), octet);
        val = (val << 8U) | static_cast<std::uint8_t>(octet);
        ++components;
        if (dot == std::string_view::npos)
            break;
        begin = dot + 1;
    }
    if (components != 4)
        return false;
    *out = val;
    return true;
}

// Parses "a.b.c.d/n" (or a bare "a.b.c.d" as /32). Rejects anything else,
// including IPv6 and trailing garbage after the prefix length.
bool parseIPv4Cidr(std::string_view cidr, std::uint32_t* network, int* prefix) {
    const auto slash = cidr.find('/');
    int bits = 32;
    if (slash != std::string_view::npos) {
        const auto prefixText = cidr.substr(slash + 1);
        if (prefixText.empty() || prefixText.size() > 2)
            return false;
        const auto result = std::from_chars(
            prefixText.data(), prefixText.data() + prefixText.size(), bits);
        if (result.ec != std::errc{} ||
            result.ptr != prefixText.data() + prefixText.size() || bits < 0 ||
            bits > 32)
            return false;
    }
    std::uint32_t value = 0;
    if (!parseIPv4Value(cidr.substr(0, slash), &value))
        return false;
    *network = value;
    *prefix = bits;
    return true;
}

std::uint32_t prefixMask(int prefix) {
    return prefix == 0 ? 0U : (~0U << (32 - prefix));
}

// Longest-prefix length of `cidr` covering `candidate`, or -1.
int cidrMatchLength(std::uint32_t candidate, std::string_view cidr) {
    std::uint32_t network = 0;
    int prefix = 0;
    if (!parseIPv4Cidr(cidr, &network, &prefix))
        return -1;
    const auto mask = prefixMask(prefix);
    return (candidate & mask) == (network & mask) ? prefix : -1;
}

std::string firstIPv4(const std::vector<std::string>& addresses) {
    for (const auto& address : addresses) {
        if (PeerDirectory::isLiteralIPv4(address))
            return address;
    }
    return {};
}

} // namespace

bool PeerDirectory::isLiteralIPv4(std::string_view address) {
    std::size_t begin = 0;
    int components = 0;
    while (begin <= address.size()) {
        const auto dot = address.find('.', begin);
        const auto end = dot == std::string_view::npos ? address.size() : dot;
        if (!parseOctet(address.substr(begin, end - begin)))
            return false;
        ++components;
        if (dot == std::string_view::npos)
            break;
        begin = dot + 1;
    }
    return components == 4;
}

bool PeerDirectory::isRoutableIPv4Subnet(
    std::string_view cidr, const std::vector<std::string>& ownAddresses) {
    std::uint32_t network = 0;
    int prefix = 0;
    if (!parseIPv4Cidr(cidr, &network, &prefix))
        return false;
    // 0.0.0.0/0 is an exit-node route; routing every address through it would
    // also capture hosts on the Switch's own LAN.
    if (prefix == 0)
        return false;
    if (prefix == 32) {
        for (const auto& own : ownAddresses) {
            std::uint32_t ownValue = 0;
            if (parseIPv4Value(own, &ownValue) && ownValue == network)
                return false;
        }
    }
    return true;
}

bool PeerDirectory::validatePeer(const Peer& peer, std::string* error) {
    if (peer.stableId.empty()) {
        if (error) *error = "peer has no stable ID";
        return false;
    }
    if (peer.endpoints.size() > kMaxEndpointsPerPeer) {
        if (error) *error = "peer endpoint limit exceeded";
        return false;
    }
    if (peer.allowedIPs.size() > kMaxAllowedIPsPerPeer) {
        if (error) *error = "peer allowed IP limit exceeded";
        return false;
    }
    for (const auto& address : peer.addresses) {
        if (address.size() > 128) {
            if (error) *error = "peer address is oversized";
            return false;
        }
    }
    return true;
}

bool PeerDirectory::replace(std::vector<Peer> peers, std::string* error) {
    if (peers.size() > kMaxPeers) {
        if (error) *error = "peer limit exceeded";
        return false;
    }
    std::unordered_map<std::string, Peer> replacement;
    replacement.reserve(peers.size());
    for (auto& peer : peers) {
        if (!validatePeer(peer, error))
            return false;
        const std::string stableId = peer.stableId;
        const auto [_, inserted] = replacement.emplace(stableId,
                                                       std::move(peer));
        if (!inserted) {
            if (error) *error = "duplicate stable peer ID";
            return false;
        }
    }
    std::unique_lock lock(mutex_);
    peers_ = std::move(replacement);
    return true;
}

bool PeerDirectory::apply(const PeerDelta& delta, std::string* error) {
    std::unordered_set<std::string> changedIds;
    changedIds.reserve(delta.changed.size());
    for (const auto& peer : delta.changed) {
        if (!validatePeer(peer, error))
            return false;
        if (!changedIds.emplace(peer.stableId).second) {
            if (error) *error = "duplicate stable peer ID in delta";
            return false;
        }
    }

    // Validate the projected directory before mutating it. Removals must be
    // accounted for first, and updates to existing peers must not consume
    // another slot.
    std::unique_lock lock(mutex_);
    std::unordered_set<std::string> projectedIds;
    projectedIds.reserve(peers_.size() + delta.changed.size());
    for (const auto& [id, _] : peers_)
        projectedIds.emplace(id);
    for (const auto& id : delta.removedStableIds)
        projectedIds.erase(id);
    for (const auto& peer : delta.changed)
        projectedIds.emplace(peer.stableId);
    if (projectedIds.size() > kMaxPeers) {
        if (error) *error = "peer limit exceeded";
        return false;
    }

    for (const auto& id : delta.removedStableIds)
        peers_.erase(id);
    for (const auto& peer : delta.changed)
        peers_.insert_or_assign(peer.stableId, peer);
    // Presence patches apply to whatever the directory holds after the
    // removals and updates above. Unknown IDs are stale races (the peer left
    // between frames) and are skipped, never failed.
    for (const auto& change : delta.onlineChanges) {
        const auto found = peers_.find(change.stableId);
        if (found != peers_.end())
            found->second.online = change.online;
    }
    return true;
}

std::vector<Peer> PeerDirectory::snapshot() const {
    std::shared_lock lock(mutex_);
    std::vector<Peer> result;
    result.reserve(peers_.size());
    for (const auto& [_, peer] : peers_)
        result.push_back(peer);
    return result;
}

std::optional<Peer> PeerDirectory::findByStableId(
    std::string_view stableId) const {
    std::shared_lock lock(mutex_);
    const auto found = peers_.find(std::string(stableId));
    return found == peers_.end() ? std::nullopt
                                 : std::optional<Peer>(found->second);
}

std::optional<RemoteRouteTarget> PeerDirectory::resolveIPv4(
    std::string_view address) const {
    std::uint32_t candidate = 0;
    if (!parseIPv4Value(address, &candidate))
        return std::nullopt;
    std::shared_lock lock(mutex_);
    const std::string addrStr(address);

    // 1. Direct match on a peer's own tailnet address (100.x.y.z).
    for (const auto& [id, peer] : peers_) {
        for (const auto& own : peer.addresses) {
            if (own == address) {
                return RemoteRouteTarget{id, own, addrStr, "127.0.0.1",
                                         RemoteRouteMode::Proxy};
            }
        }
    }

    // 2. Subnet route advertised by a peer (e.g. an OpenWrt subnet router
    //    sharing its LAN). The WireGuard session is with the router
    //    (peerAddress); the relay dials the LAN host (targetAddress) and the
    //    router forwards it. Longest prefix wins, as in Tailscale itself.
    const Peer* best = nullptr;
    const std::string* bestId = nullptr;
    int bestPrefix = -1;
    for (const auto& [id, peer] : peers_) {
        for (const auto& subnet : peer.allowedIPs) {
            const int prefix = cidrMatchLength(candidate, subnet);
            if (prefix > bestPrefix ||
                (prefix == bestPrefix && prefix >= 0 && best &&
                 !best->online && peer.online)) {
                if (firstIPv4(peer.addresses).empty())
                    continue;
                best = &peer;
                bestId = &id;
                bestPrefix = prefix;
            }
        }
    }
    if (best) {
        return RemoteRouteTarget{*bestId, firstIPv4(best->addresses), addrStr,
                                 "127.0.0.1", RemoteRouteMode::Proxy};
    }

    return std::nullopt;
}

} // namespace artemis::tailscale
