#include "../app/src/remote_access/RemoteAccessManager.hpp"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <chrono>

namespace {

class BlockingProvider final : public IRemoteAccessProvider {
public:
    std::string id() const override { return "blocking"; }
    std::string name() const override { return "Blocking"; }
    bool available() const override { return true; }
    bool start() override { return true; }
    void stop() override { ++stops; }
    std::string status() const override { return "ready"; }
    std::string lastError() const override { return {}; }
    std::string localAddress() const override { return {}; }
    std::vector<RemoteAccessPeer> peers() const override { return {}; }
    std::optional<RemoteRouteTarget>
    resolveRoute(std::string_view address) const override {
        const std::string value(address);
        return RemoteRouteTarget{value, value, value, "127.0.0.1",
                                 RemoteRouteMode::Proxy};
    }
    bool activateRoute(const RemoteRouteTarget&) override {
        std::unique_lock lock(mutex);
        entered = true;
        changed.notify_all();
        changed.wait(lock, [this] { return released; });
        ++activations;
        return true;
    }
    void deactivateRoute(const RemoteRouteTarget&) override {
        ++deactivations;
    }
    bool routesAreExclusive() const override { return true; }

    void waitUntilEntered() {
        std::unique_lock lock(mutex);
        changed.wait(lock, [this] { return entered; });
    }
    void releaseActivation() {
        std::lock_guard lock(mutex);
        released = true;
        changed.notify_all();
    }

    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    bool released = false;
    std::atomic<int> activations{0};
    std::atomic<int> deactivations{0};
    std::atomic<int> stops{0};
};

} // namespace

int main() {
    auto& manager = RemoteAccessManager::instance();
    auto owned = std::make_unique<BlockingProvider>();
    auto* provider = owned.get();
    manager.registerProvider(std::move(owned));
    assert(manager.selectAndStartProvider("blocking").started);

    const auto target = *provider->resolveRoute("100.64.0.7");
    std::atomic<bool> activated{true};
    std::thread routeThread([&] {
        activated = manager.activateRoute("blocking", target);
    });
    provider->waitUntilEntered();

    // Stop invalidates manager state immediately, then waits for the provider
    // operation boundary. It must never call stop concurrently with route
    // activation.
    std::thread stopThread([&] { manager.stopActiveProvider(); });
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(1);
    while (!manager.activeProviderId().empty() &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    assert(manager.activeProviderId().empty());
    assert(provider->stops == 0);
    provider->releaseActivation();
    routeThread.join();
    stopThread.join();

    assert(!activated);
    assert(provider->activations == 1);
    assert(provider->deactivations == 1);
    assert(provider->stops == 1);
    assert(manager.activeProviderId().empty());
    return 0;
}
