#include "TailscaleCore.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

using artemis::tailscale::Identity;
using artemis::tailscale::Key32;
using artemis::tailscale::Peer;
using artemis::tailscale::PeerDelta;
using artemis::tailscale::SecureBytes;
using artemis::tailscale::Snapshot;
using artemis::tailscale::TailscaleCore;

namespace {

Key32 sequential(std::uint8_t start) {
    Key32 key{};
    for (std::size_t i = 0; i < key.size(); ++i)
        key[i] = static_cast<std::uint8_t>(start + i);
    return key;
}

// A deterministic control session that hands the engine a full netmap on the
// first poll and a single incremental delta on the second, then reports
// disconnect. This exercises both the wholesale replace path and the delta path.
class FakeControl final : public artemis::tailscale::IControlSession {
public:
    bool connect(const Identity& identity,
                 std::span<const std::uint8_t> authKey,
                 std::string* error) override {
        (void)identity;
        if (authKey.empty()) {
            if (error) *error = "missing auth key";
            return false;
        }
        connected_ = true;
        return true;
    }

    bool poll(PeerDelta* delta,
              std::optional<std::vector<Peer>>* fullPeers,
              std::string* localAddress, std::string* error) override {
        if (!connected_) {
            if (error) *error = "not connected";
            return false;
        }
        if (pollCount_ == 0) {
            Peer alpha;
            alpha.stableId = "ts-alpha";
            alpha.nodeKey = sequential(1);
            alpha.addresses = {"100.64.0.1"};
            alpha.homeDerp = 3;
            Peer beta;
            beta.stableId = "ts-beta";
            beta.nodeKey = sequential(33);
            beta.addresses = {"100.64.0.2"};
            beta.homeDerp = 3;
            *fullPeers = std::vector<Peer>{alpha, beta};
            *localAddress = "100.101.102.103";
            pollCount_ = 1;
            return true;
        }
        if (pollCount_ == 1) {
            Peer gamma;
            gamma.stableId = "ts-gamma";
            gamma.nodeKey = sequential(65);
            gamma.addresses = {"100.64.0.3"};
            gamma.homeDerp = 3;
            delta->changed = {gamma};
            delta->removedStableIds = {"ts-beta"};
            *localAddress = "100.101.102.103";
            pollCount_ = 2;
            return true;
        }
        // Keep-alive: the session stays healthy after the delta is applied.
        (void)error;
        return true;
    }

    void close() noexcept override {}

private:
    bool connected_ = false;
    int pollCount_ = 0;
};

class FakeOverlay final : public artemis::tailscale::IOverlayRoute {
public:
    bool start(const RemoteRouteTarget& target, std::string* error) override {
        (void)target;
        (void)error;
        started_ = true;
        return true;
    }
    bool prepareForStreaming(const RemoteRouteTarget& target,
                             std::string* error) override {
        (void)target;
        (void)error;
        return started_;
    }
    void stop() noexcept override { started_ = false; }
    bool wasStarted() const { return started_; }

private:
    bool started_ = false;
};

bool waitFor(std::function<bool()> predicate, int milliseconds) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(milliseconds);
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return predicate();
}

std::optional<Peer> findPeer(const Snapshot& snapshot,
                             const std::string& stableId) {
    for (const auto& peer : snapshot.peers)
        if (peer.stableId == stableId) return peer;
    return std::nullopt;
}

} // namespace

int main() {
    const auto statePath =
        std::filesystem::temp_directory_path() /
        ("artemis-tailscale-core-test-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".state");

    auto* overlay = new FakeOverlay;
    auto control = std::make_unique<FakeControl>();
    TailscaleCore core(
        statePath, std::move(control), std::unique_ptr<FakeOverlay>(overlay),
        [](Identity& identity, std::string*) {
            identity.machinePrivate = sequential(11);
            identity.nodePrivate = sequential(55);
            identity.discoPrivate = sequential(99);
            return true;
        });

    SecureBytes authKey("tskey-auth-test");
    assert(core.start(std::move(authKey), SecureBytes{}));

    // The engine must reach Ready once the full netmap is applied.
    const bool ready =
        waitFor([&] { return core.snapshot().state == Snapshot::State::Ready; },
                3000);
    if (!ready) {
        const auto s = core.snapshot();
        std::fprintf(stderr,
                     "not ready: state=%d status='%s' error='%s' peers=%zu "
                     "local='%s'\n",
                     static_cast<int>(s.state), s.status.c_str(),
                     s.lastError.c_str(), s.peers.size(), s.localAddress.c_str());
    }
    assert(ready);

    {
        auto snapshot = core.snapshot();
        assert(snapshot.localAddress == "100.101.102.103");
        assert(findPeer(snapshot, "ts-alpha").has_value());
    }

    // The delta poll replaces beta with gamma: after it settles the directory
    // must contain alpha and gamma and must have actually dropped beta.
    const bool deltaApplied = waitFor([&] {
        const auto snapshot = core.snapshot();
        return snapshot.peers.size() == 2 &&
               findPeer(snapshot, "ts-alpha").has_value() &&
               findPeer(snapshot, "ts-gamma").has_value() &&
               !findPeer(snapshot, "ts-beta").has_value();
    }, 3000);
    assert(deltaApplied);

    // resolveRoute must translate a tailnet IPv4 to the owning peer.
    auto alphaTarget = core.resolveRoute("100.64.0.1");
    assert(alphaTarget.has_value());
    assert(alphaTarget->peerId == "ts-alpha");

    // Route activation, streaming prep, and deactivation must reach the overlay.
    const auto gammaTarget = core.resolveRoute("100.64.0.3");
    assert(gammaTarget.has_value());
    assert(gammaTarget->peerId == "ts-gamma");
    assert(core.activateRoute(*gammaTarget));
    assert(core.prepareRouteForStreaming(*gammaTarget));
    assert(overlay->wasStarted());
    // Path bookkeeping must accept the new peer without throwing.
    (void)core.pathInfo("ts-gamma");

    core.deactivateRoute(*gammaTarget);
    assert(!overlay->wasStarted());

    core.stop();
    assert(core.snapshot().state == Snapshot::State::Stopped);

    std::filesystem::remove(statePath);
    return 0;
}