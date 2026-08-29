#include "TailscaleWgxRoute.hpp"

#include <cassert>
#include <string>

using artemis::tailscale::TailscaleWgxRoute;

int main() {
    // The encrypted data path is deliberately fail-closed: activating a route
    // must never succeed until real ciphertext can move to the peer.
    RemoteRouteTarget target;
    target.peerId = "ts-peer";
    target.peerAddress = "100.64.0.5";
    target.targetAddress = "100.64.0.5";

    std::string error;
    TailscaleWgxRoute route;
    assert(!route.start(target, &error));
    assert(!error.empty());
    assert(!route.prepareForStreaming(target, &error));
    // stop() must be safe to call even with no active route.
    route.stop();
    return 0;
}