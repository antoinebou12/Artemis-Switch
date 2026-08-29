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

    bool replace(std::vector<Peer> peers, std::string* error = nullptr);
    bool apply(const PeerDelta& delta, std::string* error = nullptr);
    [[nodiscard]] std::vector<Peer> snapshot() const;
    [[nodiscard]] std::optional<Peer> findByStableId(
        std::string_view stableId) const;
    [[nodiscard]] std::optional<RemoteRouteTarget> resolveIPv4(
        std::string_view address) const;

    static bool isLiteralIPv4(std::string_view address);

private:
    static bool validatePeer(const Peer& peer, std::string* error);

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Peer> peers_;
};

} // namespace artemis::tailscale
