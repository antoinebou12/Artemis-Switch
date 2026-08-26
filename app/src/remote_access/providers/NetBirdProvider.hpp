#pragma once
#include "../IRemoteAccessProvider.hpp"

// Thin wrapper around libnetbird.a. Everything protocol-related -- setup-key
// login, peer sync, relay transport, WireGuard handshake -- lives in that
// library; this class only drives its lifecycle and exposes it to Artemis.
class NetBirdProvider : public IRemoteAccessProvider {
public:
    std::string id() const override;
    std::string name() const override;
    bool available() const override;
    bool start() override;
    void stop() override;
    void poll() override;
    std::string status() const override;
    std::string lastError() const override;
    std::string localAddress() const override;
    std::vector<RemoteAccessPeer> peers() const override;
    bool activateRoute(const std::string& peerId) override;
    void deactivateRoute(const std::string& peerId) override;

private:
    std::string lastError_;
    std::string activePeer_;
    bool started_ = false;
};
