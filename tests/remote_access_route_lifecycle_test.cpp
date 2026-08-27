#include "../app/src/remote_access/RemoteAccessManager.hpp"

#include <cassert>
#include <memory>
#include <string>

namespace {

class RouteProvider final : public IRemoteAccessProvider {
public:
    std::string id() const override { return "route-test"; }
    std::string name() const override { return "Route test"; }
    bool available() const override { return true; }
    bool start() override { return true; }
    void stop() override { ++stops; }
    std::string status() const override { return "Running"; }
    std::string lastError() const override { return {}; }
    std::string localAddress() const override { return {}; }
    std::vector<RemoteAccessPeer> peers() const override { return {}; }
    bool canRouteAddress(const std::string& address) const override {
        return !address.empty();
    }

    bool activateRoute(const std::string&) override {
        ++activations;
        return true;
    }
    bool routesAreExclusive() const override { return true; }
    bool prepareRouteForStreaming(const std::string&) override {
        ++streamPreparations;
        return true;
    }
    void deactivateRoute(const std::string&) override { ++deactivations; }

    int activations = 0;
    int streamPreparations = 0;
    int deactivations = 0;
    int stops = 0;
};

} // namespace

int main() {
    auto& manager = RemoteAccessManager::instance();
    auto ownedProvider = std::make_unique<RouteProvider>();
    auto* provider = ownedProvider.get();
    manager.registerProvider(std::move(ownedProvider));

    auto selected = manager.selectAndStartProvider("route-test");
    assert(selected.started);
    assert(manager.activateRoute("route-test", "peer"));
    assert(provider->activations == 1);
    assert(manager.prepareRouteForStreaming("route-test", "peer"));
    assert(provider->streamPreparations == 1);

    // A provider restart invalidates the proxy even if an old lease object is
    // still alive. The next activation must call the provider again instead
    // of incrementing the stale reference count.
    manager.stopActiveProvider();
    assert(provider->stops == 1);
    assert(!manager.prepareRouteForStreaming("route-test", "peer"));

    selected = manager.selectAndStartProvider("route-test");
    assert(selected.started);
    assert(manager.activateRoute("route-test", "peer"));
    assert(provider->activations == 2);

    manager.deactivateRoute("route-test", "peer");
    assert(provider->deactivations == 1);

    assert(manager.activateRoute("route-test", "peer-a"));
    assert(manager.activateRoute("route-test", "peer-b"));
    assert(provider->activations == 4);
    assert(!manager.prepareRouteForStreaming("route-test", "peer-a"));
    assert(manager.prepareRouteForStreaming("route-test", "peer-b"));

    // Releasing the stale peer-a lease must not tear down peer-b's route.
    manager.deactivateRoute("route-test", "peer-a");
    assert(provider->deactivations == 1);
    manager.deactivateRoute("route-test", "peer-b");
    assert(provider->deactivations == 2);
    return 0;
}
