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
           hexKey(33) + R"(","Addresses":["100.64.0.2/32"],)"
           // beta is a subnet router (e.g. OpenWrt) that is also an exit node.
           R"("AllowedIPs":["100.64.0.2/32","fd7a:115c:a1e0::2/128","192.168.1.0/24","0.0.0.0/0","::/0"],)"
           R"("HomeDERP":3,"Online":false}]})";
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
    std::optional<std::vector<artemis::tailscale::DerpRegion>> derpMap;
    assert(session.poll(&delta, &fullPeers, &localAddress, &derpMap, &error));
    assert(fullPeers.has_value());
    assert(fullPeers->size() == 2);
    assert((*fullPeers)[0].stableId == "ts-alpha");
    assert((*fullPeers)[1].stableId == "ts-beta");
    // Only the LAN subnet is kept as a route: the peer's own addresses, IPv6
    // and exit-node defaults are dropped. Addresses are left untouched.
    assert((*fullPeers)[0].allowedIPs.empty());
    assert((*fullPeers)[1].allowedIPs ==
           std::vector<std::string>{"192.168.1.0/24"});
    assert((*fullPeers)[1].addresses ==
           std::vector<std::string>{"100.64.0.2"});
    assert(localAddress == "100.101.102.103");
    assert(delta.changed.empty());
    // No DERPMap section in this frame: the out-param stays disengaged.
    assert(!derpMap.has_value());

    // Second poll delivers the incremental delta (beta removed, gamma added).
    assert(session.poll(&delta, &fullPeers, &localAddress, &derpMap, &error));
    assert(!fullPeers.has_value());
    assert(delta.removedStableIds.size() == 1);
    assert(delta.removedStableIds[0] == "ts-beta");
    assert(delta.changed.size() == 1);
    assert(delta.changed[0].stableId == "ts-gamma");

    // Third poll is a keep-alive: empty contract, still healthy.
    delta.changed.clear();
    delta.removedStableIds.clear();
    assert(session.poll(&delta, &fullPeers, &localAddress, &derpMap, &error));
    assert(!fullPeers.has_value() && delta.changed.empty() &&
           delta.removedStableIds.empty());

    session.close();

    // --- HTTP/2 flow control ------------------------------------------------
    using artemis::tailscale::Http2FrameDecoder;
    using artemis::tailscale::Http2ReceiveWindow;
    using artemis::tailscale::kHttp2ClientConnectionWindow;
    using artemis::tailscale::kHttp2ClientStreamWindow;
    using artemis::tailscale::kHttp2DefaultWindow;
    {
        // Preface = magic, SETTINGS(INITIAL_WINDOW_SIZE), WINDOW_UPDATE(0).
        const auto preface = artemis::tailscale::buildHttp2ClientPreface();
        assert(preface.size() == 24 + 9 + 6 + 9 + 4);
        Http2FrameDecoder decoder;
        assert(decoder.append(std::span<const std::uint8_t>(
            preface.data() + 24, preface.size() - 24)));
        const auto settings = decoder.take();
        assert(settings && settings->type == 0x04 && settings->flags == 0 &&
               settings->streamId == 0 && settings->payload.size() == 6);
        assert(settings->payload[0] == 0x00 && settings->payload[1] == 0x04);
        const std::uint32_t streamWindow =
            (std::uint32_t{settings->payload[2]} << 24U) |
            (std::uint32_t{settings->payload[3]} << 16U) |
            (std::uint32_t{settings->payload[4]} << 8U) | settings->payload[5];
        assert(streamWindow == kHttp2ClientStreamWindow);
        const auto update = decoder.take();
        assert(update && update->type == 0x08 && update->streamId == 0 &&
               update->payload.size() == 4);
        const std::uint32_t increment =
            (std::uint32_t{update->payload[0]} << 24U) |
            (std::uint32_t{update->payload[1]} << 16U) |
            (std::uint32_t{update->payload[2]} << 8U) | update->payload[3];
        assert(increment == kHttp2ClientConnectionWindow - kHttp2DefaultWindow);
        assert(!decoder.take());
    }
    {
        const auto frame = artemis::tailscale::buildHttp2WindowUpdate(3, 70000);
        const std::vector<std::uint8_t> expected{
            0, 0, 4, 0x08, 0, 0, 0, 0, 3, 0x00, 0x01, 0x11, 0x70};
        assert(frame == expected);
    }
    {
        // Replenish once half the window is used, never before, and carry
        // the exact number of consumed bytes.
        Http2ReceiveWindow window(1000);
        assert(!window.consume(0));
        assert(!window.consume(499));
        const auto first = window.consume(1);
        assert(first && *first == 500);
        assert(!window.consume(100));
        const auto second = window.consume(700);
        assert(second && *second == 800);
        window.consume(300);
        window.reset();
        assert(!window.consume(499));
    }
    return 0;
}
