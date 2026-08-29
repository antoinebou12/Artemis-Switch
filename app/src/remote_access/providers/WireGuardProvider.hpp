#pragma once
#include "../IRemoteAccessProvider.hpp"
#include <string>

class WireGuardProvider : public IRemoteAccessProvider {
public:
    std::string id() const override { return "wireguard"; }
    std::string name() const override { return "WireGuard"; }
    bool available() const override;

    bool start() override;
    void stop() override;
    void poll() override {}

    std::string status() const override;
    std::string lastError() const override;

    std::string localAddress() const override;
    std::vector<RemoteAccessPeer> peers() const override;

    std::optional<RemoteRouteTarget>
    resolveRoute(std::string_view address) const override;
    bool activateRoute(const RemoteRouteTarget& target) override;
    bool routesAreExclusive() const override { return true; }
    bool prepareRouteForStreaming(const RemoteRouteTarget& target) override;
    void deactivateRoute(const RemoteRouteTarget& target) override;
};
