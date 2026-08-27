#include <cassert>
#include <memory>
#include <string>

#include "../app/src/remote_access/RemoteAccessManager.hpp"

namespace {

class FakeProvider final : public IRemoteAccessProvider {
public:
    FakeProvider(std::string providerId, bool isAvailable, bool startResult)
        : providerId(std::move(providerId)), isAvailable(isAvailable),
          startResult(startResult) {}

    std::string id() const override { return providerId; }
    std::string name() const override { return providerId; }
    bool available() const override { return isAvailable; }
    bool start() override {
        ++starts;
        running = startResult;
        return startResult;
    }
    void stop() override {
        ++stops;
        running = false;
    }
    void poll() override { ++polls; }
    std::string status() const override {
        if (!isAvailable) return "Unavailable";
        return running ? "Running" : "Error";
    }
    std::string lastError() const override { return {}; }
    std::string localAddress() const override { return {}; }
    std::vector<RemoteAccessPeer> peers() const override { return {}; }
    bool activateRoute(const std::string&) override { return false; }
    void deactivateRoute(const std::string&) override {}

    std::string providerId;
    bool isAvailable = false;
    bool startResult = false;
    bool running = false;
    int starts = 0;
    int stops = 0;
    int polls = 0;
};

} // namespace

int main() {
    auto& manager = RemoteAccessManager::instance();

    auto alphaOwned = std::make_unique<FakeProvider>("alpha", true, true);
    auto* alpha = alphaOwned.get();
    manager.registerProvider(std::move(alphaOwned));

    auto unavailableOwned =
        std::make_unique<FakeProvider>("unavailable", false, false);
    auto* unavailable = unavailableOwned.get();
    manager.registerProvider(std::move(unavailableOwned));

    auto failingOwned =
        std::make_unique<FakeProvider>("failing", true, false);
    auto* failing = failingOwned.get();
    manager.registerProvider(std::move(failingOwned));

    auto result = manager.selectAndStartProvider("alpha");
    assert(result.found && result.available && result.started);
    assert(manager.activeProviderId() == "alpha");
    assert(alpha->starts == 1 && alpha->stops == 0);

    // Config-path changes deliberately restart the selected provider.
    result = manager.selectAndStartProvider("alpha");
    assert(result.started);
    assert(alpha->starts == 2 && alpha->stops == 1);

    result = manager.selectAndStartProvider("unavailable");
    assert(result.found && !result.available && !result.started);
    assert(alpha->stops == 2);
    assert(unavailable->starts == 0);
    assert(manager.activeProviderId().empty());

    result = manager.selectAndStartProvider("failing");
    assert(result.found && result.available && !result.started);
    assert(failing->starts == 1);
    assert(manager.activeProviderId().empty());

    // Polling does not implement hidden startup retries.
    manager.poll();
    assert(alpha->starts == 2);
    assert(unavailable->starts == 0);
    assert(failing->starts == 1);
    assert(alpha->polls == 1 && unavailable->polls == 1 && failing->polls == 1);

    result = manager.selectAndStartProvider("");
    assert(result.found && result.available && !result.started);
    assert(manager.activeProviderId().empty());
    return 0;
}
