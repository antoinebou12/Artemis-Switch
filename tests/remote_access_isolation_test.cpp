#include "../app/src/remote_access/RemoteAccessManager.hpp"
#include "../app/src/remote_access/RemoteRouteLease.hpp"

#include <cassert>
#include <memory>
#include <string>
#include <vector>

class FakeProvider final : public IRemoteAccessProvider {
public:
    explicit FakeProvider(std::string providerId, bool exclusive)
        : providerId_(std::move(providerId)), exclusive_(exclusive) {}

    std::string id() const override { return providerId_; }
    std::string name() const override { return providerId_; }
    bool available() const override { return true; }
    bool start() override { ++starts; return true; }
    void stop() override { ++stops; }
    std::string status() const override { return "ready"; }
    std::string lastError() const override { return {}; }
    std::string localAddress() const override { return "100.64.0.2"; }
    std::vector<RemoteAccessPeer> peers() const override { return {}; }
    bool canRouteAddress(const std::string& address) const override {
        return address.rfind("100.", 0) == 0;
    }
    bool activateRoute(const std::string& peerId) override {
        activePeer = peerId;
        ++activations;
        return true;
    }
    void deactivateRoute(const std::string& peerId) override {
        assert(peerId == activePeer);
        activePeer.clear();
        ++deactivations;
    }
    bool routesAreExclusive() const override { return exclusive_; }
    bool prepareRouteForStreaming(const std::string& peerId) override {
        ++prepares;
        return peerId == activePeer;
    }

    int starts = 0;
    int stops = 0;
    int activations = 0;
    int deactivations = 0;
    int prepares = 0;
    std::string activePeer;

private:
    std::string providerId_;
    bool exclusive_;
};

int main() {
    auto& manager = RemoteAccessManager::instance();
    auto wireGuard = std::make_unique<FakeProvider>("wireguard", true);
    auto netBird = std::make_unique<FakeProvider>("netbird", true);
    auto* wireGuardPtr = wireGuard.get();
    auto* netBirdPtr = netBird.get();
    manager.registerProvider(std::move(wireGuard));
    manager.registerProvider(std::move(netBird));

    assert(manager.selectAndStartProvider("wireguard").started);
    assert(wireGuardPtr->starts == 1);
    assert(wireGuardPtr->canRouteAddress("100.115.188.144"));
    assert(!wireGuardPtr->canRouteAddress("host.example"));

    RemoteRouteLease first(manager, "wireguard", "100.115.188.144",
                           "100.115.188.144", "127.0.0.1");
    assert(first.isActive());
    assert(first.prepareForStreaming());
    assert(wireGuardPtr->prepares == 1);

    // An exclusive provider must activate the new target instead of retaining
    // the old lease count as valid state.
    RemoteRouteLease second(manager, "wireguard", "100.115.188.145",
                            "100.115.188.145", "127.0.0.1");
    assert(second.isActive());
    assert(wireGuardPtr->activations == 2);
    first.release();
    assert(wireGuardPtr->activePeer == "100.115.188.145");
    second.release();
    assert(wireGuardPtr->deactivations == 1);

    assert(manager.selectAndStartProvider("netbird").started);
    assert(wireGuardPtr->stops == 1);
    assert(netBirdPtr->starts == 1);
    assert(manager.activeProviderId() == "netbird");
    manager.stopActiveProvider();
    assert(netBirdPtr->stops == 1);
    assert(manager.activeProviderId().empty());
    return 0;
}
