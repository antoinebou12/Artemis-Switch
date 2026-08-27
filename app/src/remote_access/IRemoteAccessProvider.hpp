#pragma once
#include <string>
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

class IRemoteAccessProvider {
public:
    virtual ~IRemoteAccessProvider() = default;

    virtual std::string id() const = 0;
    virtual std::string name() const = 0;
    virtual bool available() const = 0;

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

    virtual bool activateRoute(const std::string& peerId) = 0;
    virtual void deactivateRoute(const std::string& peerId) = 0;

    // True when activating one peer replaces the provider's previous target.
    // Multi-route providers keep the default and retain independent leases.
    virtual bool routesAreExclusive() const { return false; }

    // Starts resources needed only by the live stream (for example UDP media
    // relays). Keeping this separate from activateRoute() lets discovery,
    // pairing, and app-list requests use a much smaller socket/thread budget.
    virtual bool prepareRouteForStreaming(const std::string& peerId) {
        (void)peerId;
        return true;
    }
};
