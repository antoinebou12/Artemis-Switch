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
        const auto [_, inserted] = replacement.emplace(peer.stableId,
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
    if (!isLiteralIPv4(address))
        return std::nullopt;
    std::shared_lock lock(mutex_);
    for (const auto& [id, peer] : peers_) {
        for (const auto& candidate : peer.addresses) {
            if (candidate == address) {
                return RemoteRouteTarget{id, candidate, std::string(address),
                                         "127.0.0.1",
                                         RemoteRouteMode::Proxy};
            }
        }
    }
    return std::nullopt;
}

} // namespace artemis::tailscale
