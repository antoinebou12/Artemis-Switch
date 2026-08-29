#include "TailscaleControlSession.hpp"

#include "TailscaleTypes.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using artemis::tailscale::Identity;
using artemis::tailscale::Key32;
using artemis::tailscale::Peer;
using artemis::tailscale::PeerDelta;
using artemis::tailscale::SecureBytes;
using artemis::tailscale::TailscaleControlSession;

namespace {

std::string hexKey(std::uint8_t start) {
    constexpr char kHex[] = "0123456789abcdef";
    Key32 key{};
    for (std::size_t i = 0; i < key.size(); ++i)
        key[i] = static_cast<std::uint8_t>(start + i);
    std::string out;
    for (const auto byte : key) {
        out.push_back(kHex[byte >> 4U]);
        out.push_back(kHex[byte & 0x0fU]);
    }
    return out;
}

std::string fullNetmap() {
    return R"({"Node":{"StableID":"ts-local","Addresses":["100.101.102.103/32"],"Key":"nodekey:)" +
           hexKey(7) + R"("},"Peers":[{"StableID":"ts-alpha","ID":1,"Name":"alpha","Key":"nodekey:)" +
           hexKey(11) + R"(","DiscoKey":"discokey:)" + hexKey(22) +
           R"(","Addresses":["100.64.0.1/32"],"Endpoints":["1.2.3.4:41641"],"HomeDERP":3,"Online":true},)"
           R"({"StableID":"ts-beta","ID":2,"Name":"beta","Key":"nodekey:)" +
           hexKey(33) + R"(","Addresses":["100.64.0.2/32"],"HomeDERP":3,"Online":false}]})";
}

std::string deltaNetmap() {
    return R"({"Node":{"StableID":"ts-local","Addresses":["100.101.102.103/32"],"Key":"nodekey:)" +
           hexKey(7) + R"("},"PeersRemoved":[2],"PeersChanged":[{"StableID":"ts-gamma","ID":3,"Name":"gamma","Key":"nodekey:)" +
           hexKey(65) + R"(","Addresses":["100.64.0.3/32"],"HomeDERP":3,"Online":true}]})";
}

} // namespace

int main() {
    std::deque<std::string> records{fullNetmap(), deltaNetmap(), R"({"KeepAlive":true})"};
    TailscaleControlSession session(
        []() -> std::unique_ptr<artemis::tailscale::ITransport> { return nullptr; },
        "", 0, Key32{}, "test-host",
        [&records](std::string* record, std::string* error) {
            if (records.empty()) {
                if (error) *error = "test stream exhausted";
                return false;
            }
            *record = records.front();
            records.pop_front();
            return true;
        });

    Identity identity;
    identity.machinePrivate = Key32{};
    identity.nodePrivate = Key32{};
    identity.discoPrivate = Key32{};
    identity.machinePrivate[0] = 1;

    SecureBytes authKey("tskey-auth-test");
    std::string error;
    assert(session.connect(identity, authKey.view(), &error));
    assert(error.empty());

    // First poll delivers the full netmap.
    PeerDelta delta;
    std::optional<std::vector<Peer>> fullPeers;
    std::string localAddress;
    assert(session.poll(&delta, &fullPeers, &localAddress, &error));
    assert(fullPeers.has_value());
    assert(fullPeers->size() == 2);
    assert((*fullPeers)[0].stableId == "ts-alpha");
    assert((*fullPeers)[1].stableId == "ts-beta");
    assert(localAddress == "100.101.102.103");
    assert(delta.changed.empty());

    // Second poll delivers the incremental delta (beta removed, gamma added).
    assert(session.poll(&delta, &fullPeers, &localAddress, &error));
    assert(!fullPeers.has_value());
    assert(delta.removedStableIds.size() == 1);
    assert(delta.removedStableIds[0] == "ts-beta");
    assert(delta.changed.size() == 1);
    assert(delta.changed[0].stableId == "ts-gamma");

    // Third poll is a keep-alive: empty contract, still healthy.
    delta.changed.clear();
    delta.removedStableIds.clear();
    assert(session.poll(&delta, &fullPeers, &localAddress, &error));
    assert(!fullPeers.has_value() && delta.changed.empty() &&
           delta.removedStableIds.empty());

    session.close();
    return 0;
}
