#pragma once

#include "TailscaleTypes.hpp"

#include <optional>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>

namespace artemis::tailscale {

class PeerDirectory {
public:
    static constexpr std::size_t kMaxPeers = 1024;
    static constexpr std::size_t kMaxEndpointsPerPeer = 32;
    static constexpr std::size_t kMaxAllowedIPsPerPeer = 64;

    bool replace(std::vector<Peer> peers, std::string* error = nullptr);
    bool apply(const PeerDelta& delta, std::string* error = nullptr);
    [[nodiscard]] std::vector<Peer> snapshot() const;
    [[nodiscard]] std::optional<Peer> findByStableId(
        std::string_view stableId) const;
    [[nodiscard]] std::optional<RemoteRouteTarget> resolveIPv4(
        std::string_view address) const;

    static bool isLiteralIPv4(std::string_view address);

    // True for an IPv4 "a.b.c.d/n" subnet route worth routing through the
    // advertising peer: rejects IPv6, malformed text, exit-node defaults
    // (/0) and the peer's own tailnet address.
    static bool isRoutableIPv4Subnet(std::string_view cidr,
                                     const std::vector<std::string>& ownAddresses);

private:
    static bool validatePeer(const Peer& peer, std::string* error);

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Peer> peers_;
};

} // namespace artemis::tailscale
