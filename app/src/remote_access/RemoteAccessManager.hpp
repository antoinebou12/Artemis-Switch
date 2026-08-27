#pragma once
#include "IRemoteAccessProvider.hpp"
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

    bool activateRoute(const std::string& providerId, const std::string& peerId);
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
    RemoteAccessManager() = default;
    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<IRemoteAccessProvider>> providers_;
    std::string activeProviderId_;
    std::unordered_map<RouteKey, size_t, RouteKeyHash> activeRoutes_;
};
