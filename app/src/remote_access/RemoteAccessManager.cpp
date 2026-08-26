#include "RemoteAccessManager.hpp"
#include <algorithm>

RemoteAccessManager& RemoteAccessManager::instance() {
    static RemoteAccessManager inst;
    return inst;
}

void RemoteAccessManager::registerProvider(std::unique_ptr<IRemoteAccessProvider> provider) {
    std::lock_guard<std::mutex> lock(mutex_);
    providers_.push_back(std::move(provider));
}

IRemoteAccessProvider* RemoteAccessManager::provider(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(providers_.begin(), providers_.end(),
        [&id](const auto& p){ return p->id() == id; });
    return it != providers_.end() ? it->get() : nullptr;
}

std::vector<IRemoteAccessProvider*> RemoteAccessManager::providers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<IRemoteAccessProvider*> out;
    for (auto& p : providers_) out.push_back(p.get());
    return out;
}

std::vector<RemoteAccessProviderInfo> RemoteAccessManager::availableProviders() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RemoteAccessProviderInfo> out;
    for (auto& p : providers_) {
        RemoteAccessProviderInfo info;
        info.id = p->id();
        info.name = p->name();
        info.available = p->available();
        info.compiled = true;
        info.experimental = false;
        out.push_back(std::move(info));
    }
    return out;
}

std::string RemoteAccessManager::activeProviderId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return activeProviderId_;
}

void RemoteAccessManager::setActiveProviderId(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    activeProviderId_ = id;
}

bool RemoteAccessManager::startProvider(const std::string& id) {
    if (auto* p = provider(id)) return p->start();
    return false;
}

void RemoteAccessManager::stopProvider(const std::string& id) {
    if (auto* p = provider(id)) p->stop();
}

RemoteAccessSelectionResult
RemoteAccessManager::selectAndStartProvider(const std::string& id) {
    RemoteAccessSelectionResult result;
    result.providerId = id;

    // Restarting the same provider is intentional: config-path changes use
    // this operation too, so no stale tunnel survives a configuration change.
    stopActiveProvider();

    if (id.empty()) {
        result.found = true;
        result.available = true;
        result.status = "Off";
        return result;
    }

    auto* selected = provider(id);
    if (!selected) {
        result.status = "Provider not found";
        return result;
    }

    result.found = true;
    result.available = selected->available();
    if (!result.available) {
        result.status = selected->status();
        return result;
    }

    result.started = selected->start();
    result.status = selected->status();
    if (result.started)
        setActiveProviderId(id);
    return result;
}

void RemoteAccessManager::stopActiveProvider() {
    std::string active;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        active.swap(activeProviderId_);
    }
    if (!active.empty())
        stopProvider(active);
}

void RemoteAccessManager::poll() {
    for (auto* p : providers()) p->poll();
}

std::vector<RemoteAccessPeer> RemoteAccessManager::allPeers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<RemoteAccessPeer> out;
    for (auto& p : providers_) {
        auto peers = p->peers();
        out.insert(out.end(), peers.begin(), peers.end());
    }
    return out;
}

std::string RemoteAccessManager::status() const {
    const auto active = activeProviderId();
    if (active.empty()) return "No provider active";
    if (auto* p = provider(active)) return p->status();
    return "Provider not found";
}

bool RemoteAccessManager::activateRoute(const std::string& providerId, const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto providerIt = std::find_if(providers_.begin(), providers_.end(),
        [&providerId](const auto& p) { return p->id() == providerId; });
    if (providerIt != providers_.end()) {
        auto* p = providerIt->get();
        RouteKey key{providerId, peerId};
        auto it = activeRoutes_.find(key);
        if (it == activeRoutes_.end()) {
            bool ok = p->activateRoute(peerId);
            if (!ok) return false;
            activeRoutes_[key] = 1;
            return true;
        } else {
            ++it->second;
            return true;
        }
    }
    return false;
}

void RemoteAccessManager::deactivateRoute(const std::string& providerId, const std::string& peerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    RouteKey key{providerId, peerId};
    auto it = activeRoutes_.find(key);
    if (it != activeRoutes_.end()) {
        if (it->second > 1) {
            --it->second;
        } else {
            auto providerIt = std::find_if(providers_.begin(), providers_.end(),
                [&providerId](const auto& p) { return p->id() == providerId; });
            if (providerIt != providers_.end())
                providerIt->get()->deactivateRoute(peerId);
            activeRoutes_.erase(it);
        }
    }
}
