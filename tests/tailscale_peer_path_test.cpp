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
    std::string error;
    assert(peers.replace({first, second}, &error));
    assert(peers.resolveIPv4("100.64.0.10")->peerId == "node-1");
    assert(!peers.resolveIPv4("fd7a:115c:a1e0::10"));
    assert(!peers.resolveIPv4("host.example"));
    assert(!PeerDirectory::isLiteralIPv4("100.64.0.999"));

    PeerDelta delta;
    first.addresses = {"100.64.0.12"};
    delta.changed.push_back(first);
    delta.removedStableIds.push_back("node-2");
    assert(peers.apply(delta, &error));
    assert(!peers.resolveIPv4("100.64.0.10"));
    assert(peers.resolveIPv4("100.64.0.12"));
    assert(peers.snapshot().size() == 1);

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
