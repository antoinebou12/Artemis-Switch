#pragma once
#include "IRemoteAccessProvider.hpp"
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct RemoteAccessProviderInfo {
    std::string id;
    std::string name;
    bool compiled = true;
    bool available = false;
    bool experimental = false;
};

struct RemoteAccessSelectionResult {
    std::string providerId;
    bool found = false;
    bool available = false;
    bool started = false;
    std::string status;
};

class RemoteAccessManager {
public:
    static RemoteAccessManager& instance();

    void registerProvider(std::unique_ptr<IRemoteAccessProvider> provider);
    IRemoteAccessProvider* provider(const std::string& id) const;
    std::vector<IRemoteAccessProvider*> providers() const;
    std::vector<RemoteAccessProviderInfo> availableProviders() const;

    std::string activeProviderId() const;
    void setActiveProviderId(const std::string& id);

    bool startProvider(const std::string& id);
    void stopProvider(const std::string& id);
    RemoteAccessSelectionResult selectAndStartProvider(const std::string& id);
    void stopActiveProvider();
    void poll();

    std::vector<RemoteAccessPeer> allPeers() const;
    std::string status() const;

    bool activateRoute(const std::string& providerId,
                       const RemoteRouteTarget& target);
    bool prepareRouteForStreaming(const std::string& providerId,
                                  const std::string& peerId);
    void deactivateRoute(const std::string& providerId, const std::string& peerId);

    // Reference counted route activation
    struct RouteKey {
        std::string providerId;
        std::string peerId;
        bool operator==(const RouteKey& o) const noexcept {
            return providerId == o.providerId && peerId == o.peerId;
        }
    };
    struct RouteKeyHash {
        std::size_t operator()(const RouteKey& k) const noexcept {
            return std::hash<std::string>{}(k.providerId) ^ (std::hash<std::string>{}(k.peerId) << 1);
        }
    };

private:
    struct ProviderSlot {
        explicit ProviderSlot(std::shared_ptr<IRemoteAccessProvider> value)
            : provider(std::move(value)) {}

        std::shared_ptr<IRemoteAccessProvider> provider;
        // Serializes every lifecycle and route mutation for this provider.
        // The manager state mutex is never held while this mutex is used.
        mutable std::mutex operations;
    };

    enum class RouteState { Activating, Active, Failed };
    struct RouteEntry {
        RemoteRouteTarget target;
        std::uint64_t generation = 0;
        std::size_t references = 1;
        RouteState state = RouteState::Activating;
        std::condition_variable changed;
    };

    RemoteAccessManager() = default;
    std::shared_ptr<ProviderSlot> providerSlot(const std::string& id) const;
    std::vector<std::shared_ptr<ProviderSlot>> providerSlots() const;
    std::shared_ptr<IRemoteAccessProvider>
    providerShared(const std::string& id) const;
    void stopActiveProviderLocked();

    // Serializes provider selection/restart transactions. It is distinct from
    // mutex_, which protects only short-lived manager bookkeeping.
    mutable std::mutex lifecycleMutex_;
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<ProviderSlot>> providers_;
    std::string activeProviderId_;
    std::uint64_t providerGeneration_ = 0;
    std::unordered_map<RouteKey, std::shared_ptr<RouteEntry>, RouteKeyHash>
        activeRoutes_;
};
