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
    std::vector<Endpoint> endpoints;
    int homeDerp = 0;
    bool online = false;
};

struct PeerDelta {
    std::vector<Peer> changed;
    std::vector<std::string> removedStableIds;
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
    std::string status;
    std::string lastError;
};

} // namespace artemis::tailscale
