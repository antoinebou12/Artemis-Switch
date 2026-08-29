#pragma once

#include "TailscaleCore.hpp"

namespace artemis::tailscale {

// The encrypted packet path for Tailscale. This transport currently FAILS
// CLOSED with a clear reason: Artemis must never report a routable Tailscale
// connection until the private wgx_* instance can actually move ciphertext to
// a peer. Once that path is wired against the wgx backend, start() opens the
// tunnel and prepareForStreaming() confirms the session before accepting.
class TailscaleWgxRoute final : public IOverlayRoute {
public:
    bool start(const RemoteRouteTarget& target,
               std::string* error) override;
    bool prepareForStreaming(const RemoteRouteTarget& target,
                             std::string* error) override;
    void stop() noexcept override;
};

} // namespace artemis::tailscale