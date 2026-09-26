#include "../app/src/remote_access/tailscale/TailscalePathManager.hpp"
#include "../app/src/remote_access/tailscale/TailscalePeerDirectory.hpp"

#include <cassert>
#include <chrono>
#include <string>
#include <vector>

using namespace artemis::tailscale;

int main() {
    PeerDirectory peers;
    Peer first;
    first.stableId = "node-1";
    first.hostname = "gaming-pc";
    first.addresses = {"100.64.0.10", "fd7a:115c:a1e0::10"};
    Peer second;
    second.stableId = "node-2";
    second.addresses = {"100.64.0.11"};
    Peer openwrt;
    openwrt.stableId = "openwrt-router";
    openwrt.addresses = {"100.64.0.1"};
    openwrt.allowedIPs = {"192.168.1.0/24"};

    std::string error;
    assert(peers.replace({first, second, openwrt}, &error));
    assert(peers.resolveIPv4("100.64.0.10")->peerId == "node-1");
    assert(!peers.resolveIPv4("fd7a:115c:a1e0::10"));
    assert(!peers.resolveIPv4("host.example"));
    assert(!PeerDirectory::isLiteralIPv4("100.64.0.999"));

    // Subnet route test: LAN IP resolves through OpenWrt router peer
    auto resolvedLan = peers.resolveIPv4("192.168.1.50");
    assert(resolvedLan.has_value());
    assert(resolvedLan->peerId == "openwrt-router");
    assert(resolvedLan->peerAddress == "100.64.0.1");
    assert(resolvedLan->targetAddress == "192.168.1.50");
    assert(!peers.resolveIPv4("192.168.2.50"));

    PeerDelta delta;
    first.addresses = {"100.64.0.12"};
    delta.changed.push_back(first);
    delta.removedStableIds.push_back("node-2");
    delta.removedStableIds.push_back("openwrt-router");
    assert(peers.apply(delta, &error));
    assert(!peers.resolveIPv4("100.64.0.10"));
    assert(peers.resolveIPv4("100.64.0.12"));
    assert(peers.snapshot().size() == 1);

    // Presence patches flip the online flag without touching addresses.
    // Unknown IDs are stale races and must not fail the update.
    PeerDelta presence;
    presence.onlineChanges.push_back({"node-1", true});
    presence.onlineChanges.push_back({"gone-node", false});
    assert(peers.apply(presence, &error));
    assert(peers.findByStableId("node-1")->online);
    presence.onlineChanges.clear();
    presence.onlineChanges.push_back({"node-1", false});
    assert(peers.apply(presence, &error));
    assert(!peers.findByStableId("node-1")->online);

    // A full directory may replace one peer with another in a single delta.
    std::vector<Peer> full;
    full.reserve(PeerDirectory::kMaxPeers);
    for (std::size_t i = 0; i < PeerDirectory::kMaxPeers; ++i) {
        Peer peer;
        peer.stableId = "full-" + std::to_string(i);
        full.push_back(std::move(peer));
    }
    assert(peers.replace(std::move(full), &error));
    Peer replacement;
    replacement.stableId = "replacement";
    PeerDelta replacementDelta;
    replacementDelta.removedStableIds.push_back("full-0");
    replacementDelta.changed.push_back(replacement);
    assert(peers.apply(replacementDelta, &error));
    assert(peers.findByStableId("replacement"));

    PeerDelta duplicateDelta;
    duplicateDelta.changed = {replacement, replacement};
    assert(!peers.apply(duplicateDelta, &error));
    assert(error == "duplicate stable peer ID in delta");

    PathManager paths;
    const auto start = PathManager::Clock::time_point{};
    paths.setDerp("node-1", "tor");
    assert(paths.pathInfo("node-1").type == RemotePathType::Derp);
    paths.beginDirectProbe("node-1", "192.0.2.4:41641", start);
    paths.directPong("node-1", "192.0.2.4:41641", 17,
                     start + std::chrono::milliseconds(17));
    auto path = paths.pathInfo("node-1");
    assert(path.type == RemotePathType::DirectIPv4 && path.rttMs == 17);
    paths.poll(start + PathManager::kDirectFreshness +
               std::chrono::seconds(1));
    assert(paths.pathInfo("node-1").type == RemotePathType::Derp);
    paths.directPong("node-1", "192.0.2.4:41641", 20, start);
    paths.networkChanged();
    assert(paths.pathInfo("node-1").type == RemotePathType::Derp);
    return 0;
}
