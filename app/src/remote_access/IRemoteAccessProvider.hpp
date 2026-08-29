#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct RemoteAccessPeer {
    std::string providerId;
    std::string peerId;
    std::string name;
    std::string address;
    bool online = false;
    std::string metadata;
};

struct RemoteAccessRoute {
    std::string providerId;
    std::string peerId;
    std::string targetAddress;
    bool direct = true;
};

enum class RemoteRouteMode {
    Native,
    Proxy,
};

struct RemoteRouteTarget {
    std::string peerId;
    std::string peerAddress;
    std::string targetAddress;
    std::string connectAddress;
    RemoteRouteMode mode = RemoteRouteMode::Proxy;
};

enum class RemotePathType {
    Unknown,
    DirectIPv4,
    DirectIPv6,
    Derp,
    PeerRelayIPv4,
    PeerRelayIPv6,
};

struct RemotePathInfo {
    RemotePathType type = RemotePathType::Unknown;
    std::string endpoint;
    std::string relayRegion;
    int rttMs = -1;
};

class IRemoteAccessProvider {
public:
    virtual ~IRemoteAccessProvider() = default;

    virtual std::string id() const = 0;
    virtual std::string name() const = 0;
    virtual bool available() const = 0;
    virtual bool experimental() const noexcept { return false; }

    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void poll() {}

    virtual std::string status() const = 0;
    virtual std::string lastError() const = 0;

    virtual std::string localAddress() const = 0;
    virtual std::vector<RemoteAccessPeer> peers() const = 0;

    // BLOCKING for providers that probe the network. Never call from the UI
    // thread. Default is a no-op for providers with no peer directory.
    virtual void refreshPeers() {}

    // Resolves an address to a stable provider identity. Providers must only
    // return targets authenticated by their config or control plane.
    virtual std::optional<RemoteRouteTarget>
    resolveRoute(std::string_view address) const = 0;

    virtual bool activateRoute(const RemoteRouteTarget& target) = 0;
    virtual void deactivateRoute(const RemoteRouteTarget& target) = 0;

    // True when activating one peer replaces the provider's previous target.
    virtual bool routesAreExclusive() const { return false; }

    // Starts resources used only after a GameStream launch succeeds, such as
    // the UDP media relays.
    virtual bool prepareRouteForStreaming(const RemoteRouteTarget& target) {
        (void)target;
        return true;
    }

    virtual RemotePathInfo pathInfo(std::string_view peerId) const {
        (void)peerId;
        return {};
    }
};
