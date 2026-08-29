#pragma once

#include "../IRemoteAccessProvider.hpp"

#include <memory>
#include <mutex>
#include <string>

namespace artemis::tailscale {
class TailscaleCore;
}

// Experimental provider boundary. Protocol state runs on the portable tailscale/
// engine; this class is the Artemis-facing adapter that drives it. The engine is
// only started once a control endpoint is configured, and the encrypted data
// path stays fail-closed until a peer session is genuinely usable.
class TailscaleProvider final : public IRemoteAccessProvider {
public:
    ~TailscaleProvider() override;

    std::string id() const override { return "tailscale"; }
    std::string name() const override { return "Tailscale"; }
    bool available() const override;
    bool experimental() const noexcept override { return true; }

    bool start() override;
    void stop() override;

    std::string status() const override;
    std::string lastError() const override;
    std::string localAddress() const override;
    std::vector<RemoteAccessPeer> peers() const override;

    std::optional<RemoteRouteTarget>
    resolveRoute(std::string_view address) const override;
    bool activateRoute(const RemoteRouteTarget& target) override;
    bool prepareRouteForStreaming(const RemoteRouteTarget& target) override;
    void deactivateRoute(const RemoteRouteTarget& target) override;
    bool routesAreExclusive() const override { return true; }
    RemotePathInfo pathInfo(std::string_view peerId) const override;

    // Auth keys and passphrases are intentionally transient. UI code may hand
    // them to the provider, but Settings never serializes either value.
    void setOneOffAuthKey(std::string key);
    void setLaunchPassphrase(std::string passphrase);
    void clearTransientSecrets() noexcept;

private:
    mutable std::mutex mutex_;
    std::string status_{"Stopped"};
    std::string lastError_;
    std::string authKey_;
    std::string passphrase_;
    std::shared_ptr<artemis::tailscale::TailscaleCore> core_;
};
