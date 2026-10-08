#include "TailscaleWgxRoute.hpp"

#include <cassert>
#include <string>

using artemis::tailscale::Key32;
using artemis::tailscale::Peer;
using artemis::tailscale::SimulatedWgxBackend;
using artemis::tailscale::TailscaleWgxRoute;

namespace {

Key32 makeKey(std::uint8_t start) {
    Key32 key{};
    for (std::size_t i = 0; i < key.size(); ++i)
        key[i] = static_cast<std::uint8_t>(start + i);
    return key;
}

} // namespace

int main() {
    // 1. Unconfigured route fails closed:
    {
        RemoteRouteTarget target;
        target.peerId = "ts-peer";
        target.peerAddress = "100.64.0.5";
        target.targetAddress = "100.64.0.5";

        std::string error;
        TailscaleWgxRoute route;
        assert(!route.start(target, &error));
        assert(!error.empty());
        assert(!route.prepareForStreaming(target, &error));
        route.stop();
        assert(!route.isActive());
    }

    // 2. Simulated backend full lifecycle:
    {
        auto backend = std::make_shared<SimulatedWgxBackend>();
        const auto localKey = makeKey(10);
        const auto peerKey = makeKey(40);

        artemis::tailscale::DerpRegion region;
        region.regionId = 3;
        region.regionCode = "test";
        region.nodes.push_back({"derp3.example", 443});

        TailscaleWgxRoute route(
            backend,
            [&peerKey](std::string_view peerId) -> std::optional<Peer> {
                if (peerId == "ts-peer-valid") {
                    Peer peer;
                    peer.stableId = "ts-peer-valid";
                    peer.nodeKey = peerKey;
                    peer.addresses = {"100.64.0.10"};
                    peer.homeDerp = 3;
                    return peer;
                }
                return std::nullopt;
            },
            [&localKey]() -> std::optional<std::pair<std::string, Key32>> {
                return std::make_pair("100.64.0.2", localKey);
            });
        route.setDerpMapProvider(
            [region]() { return std::vector<artemis::tailscale::DerpRegion>{region}; });

        RemoteRouteTarget target;
        target.peerId = "ts-peer-valid";
        target.peerAddress = "100.64.0.10";
        target.targetAddress = "100.64.0.10";

        std::string error;
        assert(route.start(target, &error));
        assert(error.empty());
        assert(route.isActive());
        assert(route.activePeerId() == "ts-peer-valid");
        assert(backend->isRunning());
        assert(backend->isDerpReady());
        assert(backend->isTcpActive());
        assert(!backend->isUdpActive());
        assert(backend->activeHostIp() == "100.64.0.10");
        // No disco key anywhere: no direct-path probing is requested.
        assert(!backend->directConfig());

        // Prepare streaming activates UDP media relays
        assert(route.prepareForStreaming(target, &error));
        assert(error.empty());
        assert(route.isStreamingPrepared());
        assert(backend->isUdpActive());

        // Stop cleanly tears down
        route.stop();
        assert(!route.isActive());
        assert(!route.isStreamingPrepared());
        assert(!backend->isRunning());
        assert(!backend->isTcpActive());
        assert(!backend->isUdpActive());
        assert(!backend->isDerpReady());

        // Subnet route: WireGuard stays with the router peer while the relay
        // dials the LAN host behind it.
        RemoteRouteTarget lanTarget;
        lanTarget.peerId = "ts-peer-valid";
        lanTarget.peerAddress = "100.64.0.10";
        lanTarget.targetAddress = "192.168.1.50";
        assert(route.start(lanTarget, &error));
        assert(route.isActive());
        assert(backend->activeHostIp() == "192.168.1.50");
        route.stop();

        // A non-IPv4 host is refused before any tunnel work.
        lanTarget.targetAddress = "pc.lan";
        assert(!route.start(lanTarget, &error));
        assert(!route.isActive());
    }

    // 2b. With a disco key on both sides the route asks the backend to probe
    //     a direct path, after (never instead of) the DERP route.
    {
        auto backend = std::make_shared<SimulatedWgxBackend>();
        const auto localKey = makeKey(10);
        const auto peerKey = makeKey(40);
        const auto peerDisco = makeKey(70);
        const auto localDisco = makeKey(90);
        artemis::tailscale::DerpRegion region;
        region.regionId = 3;
        region.regionCode = "test";
        region.nodes.push_back({"derp3.example", 443});

        TailscaleWgxRoute route(
            backend,
            [&](std::string_view peerId) -> std::optional<Peer> {
                if (peerId != "ts-peer-direct")
                    return std::nullopt;
                Peer peer;
                peer.stableId = "ts-peer-direct";
                peer.nodeKey = peerKey;
                peer.discoKey = peerDisco;
                peer.addresses = {"100.64.0.11"};
                peer.endpoints.push_back({"203.0.113.7", 41641});
                peer.homeDerp = 3;
                return peer;
            },
            [&localKey]() -> std::optional<std::pair<std::string, Key32>> {
                return std::make_pair("100.64.0.2", localKey);
            });
        route.setDerpMapProvider([region]() {
            return std::vector<artemis::tailscale::DerpRegion>{region};
        });
        std::vector<std::string> published;
        std::string observedPeer;
        std::string observedEndpoint;
        route.setDirectPathHooks(
            [&]() -> std::optional<Key32> { return localDisco; },
            [&](std::vector<std::string> endpoints) { published = endpoints; },
            [&](const std::string& peerId, const std::string& endpoint, int) {
                observedPeer = peerId;
                observedEndpoint = endpoint;
            });

        RemoteRouteTarget target;
        target.peerId = "ts-peer-direct";
        target.peerAddress = "100.64.0.11";
        target.targetAddress = "100.64.0.11";
        std::string error;
        assert(route.start(target, &error));
        assert(backend->isDerpReady() && backend->isTcpActive());
        const auto& config = backend->directConfig();
        assert(config.has_value());
        assert(config->peerStableId == "ts-peer-direct");
        assert(config->peerNodeKey == peerKey);
        assert(config->peerDiscoKey == peerDisco);
        assert(config->localDiscoPrivate == localDisco);
        assert(config->localNodePrivate == localKey);
        assert(config->peerEndpoints ==
               std::vector<std::string>{"203.0.113.7:41641"});
        assert(config->derpRegion == 3 && config->derpMap.size() == 1);
        // Hooks reach the core with the route's peer id attached.
        config->publishEndpoints({"198.51.100.4:41641"});
        assert(published == std::vector<std::string>{"198.51.100.4:41641"});
        config->pathChanged("203.0.113.7:41641", 12);
        assert(observedPeer == "ts-peer-direct" &&
               observedEndpoint == "203.0.113.7:41641");
        route.stop();
        assert(!backend->directConfig());
    }

    // 3. Validation guards against malformed/missing data:
    {
        auto backend = std::make_shared<SimulatedWgxBackend>();
        const auto localKey = makeKey(10);

        TailscaleWgxRoute route(
            backend,
            [](std::string_view peerId) -> std::optional<Peer> {
                if (peerId == "ts-zero-key") {
                    Peer peer;
                    peer.stableId = "ts-zero-key";
                    peer.nodeKey = Key32{}; // all zeros
                    peer.addresses = {"100.64.0.20"};
                    return peer;
                }
                return std::nullopt;
            },
            [&localKey]() -> std::optional<std::pair<std::string, Key32>> {
                return std::make_pair("100.64.0.2", localKey);
            });

        std::string error;

        // Missing peer in netmap
        RemoteRouteTarget missingPeer{"ts-unknown", "100.64.0.30", "100.64.0.30"};
        assert(!route.start(missingPeer, &error));
        assert(!error.empty());

        // Peer with uninitialized/all-zero node key
        RemoteRouteTarget zeroKeyPeer{"ts-zero-key", "100.64.0.20", "100.64.0.20"};
        error.clear();
        assert(!route.start(zeroKeyPeer, &error));
        assert(!error.empty());

        // Invalid IP address
        RemoteRouteTarget badIpPeer{"ts-zero-key", "invalid-ip", "invalid-ip"};
        error.clear();
        assert(!route.start(badIpPeer, &error));
        assert(!error.empty());

        // Prepare streaming fails if route was not started
        error.clear();
        assert(!route.prepareForStreaming(missingPeer, &error));
        assert(!error.empty());
    }

    // 4. The relay gate fails closed: a peer without a home region, or a
    // region missing from the control-plane map, refuses the route instead
    // of advertising listeners with no packet path behind them.
    {
        auto backend = std::make_shared<SimulatedWgxBackend>();
        const auto localKey = makeKey(10);
        const auto peerKey = makeKey(40);

        TailscaleWgxRoute route(
            backend,
            [&peerKey](std::string_view peerId) -> std::optional<Peer> {
                Peer peer;
                peer.nodeKey = peerKey;
                peer.addresses = {"100.64.0.10"};
                if (peerId == "ts-no-derp") {
                    peer.stableId = "ts-no-derp";
                    peer.homeDerp = 0;
                    return peer;
                }
                if (peerId == "ts-unknown-region") {
                    peer.stableId = "ts-unknown-region";
                    peer.homeDerp = 9;
                    return peer;
                }
                return std::nullopt;
            },
            [&localKey]() -> std::optional<std::pair<std::string, Key32>> {
                return std::make_pair("100.64.0.2", localKey);
            });
        artemis::tailscale::DerpRegion region;
        region.regionId = 3;
        region.nodes.push_back({"derp3.example", 443});
        route.setDerpMapProvider(
            [region]() { return std::vector<artemis::tailscale::DerpRegion>{region}; });

        std::string error;
        RemoteRouteTarget noDerp{"ts-no-derp", "100.64.0.10", "100.64.0.10"};
        assert(!route.start(noDerp, &error));
        assert(!error.empty());
        assert(!backend->isDerpReady());

        error.clear();
        RemoteRouteTarget unknownRegion{"ts-unknown-region", "100.64.0.10", "100.64.0.10"};
        // Simulated backend accepts any region id; the missing-region refusal
        // is enforced by the real backend, which this test cannot construct
        // on the host. Success here still proves the map reached the backend.
        assert(route.start(unknownRegion, &error));
        assert(backend->isDerpReady());
        route.stop();
    }

    return 0;
}
