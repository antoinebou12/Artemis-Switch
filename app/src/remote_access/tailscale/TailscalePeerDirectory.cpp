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

bool PeerDirectory::validatePeer(const Peer& peer, std::string* error) {
    if (peer.stableId.empty()) {
        if (error) *error = "peer has no stable ID";
        return false;
    }
    if (peer.endpoints.size() > kMaxEndpointsPerPeer) {
        if (error) *error = "peer endpoint limit exceeded";
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

bool cidrMatches(std::string_view candidateIp, std::string_view cidr) {
    const auto slash = cidr.find('/');
    const auto netStr = cidr.substr(0, slash);
    int prefix = 32;
    if (slash != std::string_view::npos) {
        prefix = 0;
        for (std::size_t i = slash + 1; i < cidr.size(); ++i) {
            if (cidr[i] >= '0' && cidr[i] <= '9')
                prefix = prefix * 10 + (cidr[i] - '0');
            else
                break;
        }
    }
    if (prefix < 0 || prefix > 32)
        return false;

    auto parseIp = [](std::string_view ip, std::uint32_t* out) -> bool {
        std::size_t begin = 0;
        int components = 0;
        std::uint32_t val = 0;
        while (begin <= ip.size()) {
            const auto dot = ip.find('.', begin);
            const auto end = dot == std::string_view::npos ? ip.size() : dot;
            const auto piece = ip.substr(begin, end - begin);
            if (piece.empty() || piece.size() > 3)
                return false;
            unsigned int octet = 0;
            const auto res =
                std::from_chars(piece.data(), piece.data() + piece.size(), octet);
            if (res.ec != std::errc{} ||
                res.ptr != piece.data() + piece.size() || octet > 255)
                return false;
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
    };

    std::uint32_t candVal = 0;
    std::uint32_t netVal = 0;
    if (!parseIp(candidateIp, &candVal) || !parseIp(netStr, &netVal))
        return false;

    const std::uint32_t mask = prefix == 0 ? 0U : (~0U << (32 - prefix));
    return (candVal & mask) == (netVal & mask);
}

std::optional<RemoteRouteTarget> PeerDirectory::resolveIPv4(
    std::string_view address) const {
    if (!isLiteralIPv4(address))
        return std::nullopt;
    std::shared_lock lock(mutex_);
    const std::string addrStr(address);

    // 1. Direct match on peer's Tailscale addresses (100.x.y.z)
    for (const auto& [id, peer] : peers_) {
        for (const auto& candidate : peer.addresses) {
            if (candidate == address) {
                return RemoteRouteTarget{id, candidate, addrStr,
                                         "127.0.0.1",
                                         RemoteRouteMode::Proxy};
            }
        }
    }

    // 2. Subnet route match on advertised routes (e.g. OpenWrt router)
    for (const auto& [id, peer] : peers_) {
        for (const auto& subnet : peer.allowedIPs) {
            if (cidrMatches(address, subnet)) {
                const std::string peerAddr =
                    peer.addresses.empty() ? addrStr : peer.addresses.front();
                return RemoteRouteTarget{id, peerAddr, addrStr,
                                         "127.0.0.1",
                                         RemoteRouteMode::Proxy};
            }
        }
    }

    return std::nullopt;
}

} // namespace artemis::tailscale
