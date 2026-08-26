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

    bool activateRoute(const std::string& peerId) override { return true; }
    void deactivateRoute(const std::string& peerId) override {}
};
