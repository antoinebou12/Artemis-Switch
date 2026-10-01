#pragma once

#include "../IRemoteAccessProvider.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace artemis::tailscale {

using Key32 = std::array<std::uint8_t, 32>;

struct Identity {
    Key32 machinePrivate{};
    Key32 nodePrivate{};
    Key32 discoPrivate{};

    bool operator==(const Identity&) const = default;
};

// True when any two private keys are equal. Earlier Switch builds generated
// all three from a random source that replayed the same bytes, so such an
// identity is predictable and must be replaced.
inline bool isWeakIdentity(const Identity& identity) {
    return identity.machinePrivate == identity.nodePrivate ||
           identity.nodePrivate == identity.discoPrivate ||
           identity.machinePrivate == identity.discoPrivate;
}

struct Endpoint {
    enum class Type { Local, StunMapped, Observed };
    std::string address;
    std::uint16_t port = 0;
    Type type = Type::Local;
    std::chrono::steady_clock::time_point discoveredAt{};
    std::chrono::steady_clock::time_point lastValidatedAt{};
};

struct Peer {
    std::string stableId;
    Key32 nodeKey{};
    Key32 discoKey{};
    std::string hostname;
    std::vector<std::string> addresses;
    std::vector<std::string> allowedIPs;
    std::vector<Endpoint> endpoints;
    int homeDerp = 0;
    bool online = false;
};

struct PeerOnlineChange {
    std::string stableId;
    bool online = false;
};

struct PeerDelta {
    std::vector<Peer> changed;
    std::vector<std::string> removedStableIds;
    std::vector<PeerOnlineChange> onlineChanges;
};

// A DERP relay region from the control plane's DERPMap. The relay address a
// client dials is resolved per node: HostName when present, otherwise the
// literal IPv4. Ports default to 443 when the map omits them.
struct DerpNode {
    std::string host;
    std::uint16_t port = 443;
    // UDP STUN port on the same host; 0 when the node disables STUN.
    std::uint16_t stunPort = 3478;
};

struct DerpRegion {
    int regionId = 0;
    std::string regionCode;
    std::vector<DerpNode> nodes;
};

struct Snapshot {
    enum class State {
        Stopped,
        Starting,
        NeedsAuthentication,
        ConnectingControl,
        ConnectedControl,
        Ready,
        Error,
    };

    State state = State::Stopped;
    std::string localAddress;
    std::vector<Peer> peers;
    std::vector<DerpRegion> derpMap;
    std::string status;
    std::string lastError;
};

} // namespace artemis::tailscale
