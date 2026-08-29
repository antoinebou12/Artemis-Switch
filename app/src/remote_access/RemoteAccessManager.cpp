#include "RemoteAccessManager.hpp"
#include <algorithm>

RemoteAccessManager& RemoteAccessManager::instance() {
    static RemoteAccessManager inst;
    return inst;
}

void RemoteAccessManager::registerProvider(std::unique_ptr<IRemoteAccessProvider> provider) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto shared = std::shared_ptr<IRemoteAccessProvider>(std::move(provider));
    providers_.push_back(std::make_shared<ProviderSlot>(std::move(shared)));
}

std::shared_ptr<RemoteAccessManager::ProviderSlot>
RemoteAccessManager::providerSlot(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = std::find_if(
        providers_.begin(), providers_.end(), [&id](const auto& slot) {
            return slot->provider->id() == id;
        });
    return it == providers_.end() ? nullptr : *it;
}

std::vector<std::shared_ptr<RemoteAccessManager::ProviderSlot>>
RemoteAccessManager::providerSlots() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return providers_;
}

std::shared_ptr<IRemoteAccessProvider>
RemoteAccessManager::providerShared(const std::string& id) const {
    const auto slot = providerSlot(id);
    return slot ? slot->provider : nullptr;
}

IRemoteAccessProvider* RemoteAccessManager::provider(const std::string& id) const {
    const auto shared = providerShared(id);
    return shared.get();
}

std::vector<IRemoteAccessProvider*> RemoteAccessManager::providers() const {
    const auto slots = providerSlots();
    std::vector<IRemoteAccessProvider*> out;
    out.reserve(slots.size());
    for (const auto& slot : slots) out.push_back(slot->provider.get());
    return out;
}

std::vector<RemoteAccessProviderInfo> RemoteAccessManager::availableProviders() const {
    const auto snapshot = providerSlots();
    std::vector<RemoteAccessProviderInfo> out;
    out.reserve(snapshot.size());
    for (const auto& slot : snapshot) {
        std::lock_guard operation(slot->operations);
        const auto& p = slot->provider;
        RemoteAccessProviderInfo info;
        info.id = p->id();
        info.name = p->name();
        info.available = p->available();
        info.compiled = true;
        info.experimental = p->experimental();
        out.push_back(std::move(info));
    }
    return out;
}

std::string RemoteAccessManager::activeProviderId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return activeProviderId_;
}

void RemoteAccessManager::setActiveProviderId(const std::string& id) {
    std::lock_guard lifecycle(lifecycleMutex_);
    std::lock_guard<std::mutex> lock(mutex_);
    activeProviderId_ = id;
    ++providerGeneration_;
}

bool RemoteAccessManager::startProvider(const std::string& id) {
    std::lock_guard lifecycle(lifecycleMutex_);
    if (const auto slot = providerSlot(id)) {
        std::lock_guard operation(slot->operations);
        return slot->provider->start();
    }
    return false;
}

void RemoteAccessManager::stopProvider(const std::string& id) {
    std::lock_guard lifecycle(lifecycleMutex_);
    if (const auto slot = providerSlot(id)) {
        std::lock_guard operation(slot->operations);
        slot->provider->stop();
    }
}

RemoteAccessSelectionResult
RemoteAccessManager::selectAndStartProvider(const std::string& id) {
    std::lock_guard lifecycle(lifecycleMutex_);
    RemoteAccessSelectionResult result;
    result.providerId = id;

    // Restarting the same provider is intentional: config-path changes use
    // this operation too, so no stale tunnel survives a configuration change.
    stopActiveProviderLocked();

    if (id.empty()) {
        result.found = true;
        result.available = true;
        result.status = "Off";
        return result;
    }

    const auto selected = providerSlot(id);
    if (!selected) {
        result.status = "Provider not found";
        return result;
    }

    result.found = true;
    {
        std::lock_guard operation(selected->operations);
        result.available = selected->provider->available();
        if (!result.available) {
            result.status = selected->provider->status();
            return result;
        }

        result.started = selected->provider->start();
        result.status = selected->provider->status();
    }
    if (result.started) {
        std::lock_guard state(mutex_);
        activeProviderId_ = id;
        ++providerGeneration_;
    }
    return result;
}

void RemoteAccessManager::stopActiveProvider() {
    std::lock_guard lifecycle(lifecycleMutex_);
    stopActiveProviderLocked();
}

void RemoteAccessManager::stopActiveProviderLocked() {
    std::string active;
    std::shared_ptr<ProviderSlot> selected;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        active.swap(activeProviderId_);
        ++providerGeneration_;
        for (auto it = activeRoutes_.begin(); it != activeRoutes_.end();) {
            if (it->first.providerId == active) {
                it->second->state = RouteState::Failed;
                it->second->changed.notify_all();
                it = activeRoutes_.erase(it);
            } else {
                ++it;
            }
        }
        const auto providerIt = std::find_if(
            providers_.begin(), providers_.end(), [&active](const auto& slot) {
                return slot->provider->id() == active;
            });
        if (providerIt != providers_.end())
            selected = *providerIt;
    }
    if (selected) {
        std::lock_guard operation(selected->operations);
        selected->provider->stop();
    }
}

void RemoteAccessManager::poll() {
    for (const auto& slot : providerSlots()) {
        std::lock_guard operation(slot->operations);
        slot->provider->poll();
    }
}

std::vector<RemoteAccessPeer> RemoteAccessManager::allPeers() const {
    std::vector<RemoteAccessPeer> out;
    for (const auto& slot : providerSlots()) {
        std::lock_guard operation(slot->operations);
        auto peers = slot->provider->peers();
        out.insert(out.end(), peers.begin(), peers.end());
    }
    return out;
}

std::string RemoteAccessManager::status() const {
    const auto active = activeProviderId();
    if (active.empty()) return "No provider active";
    if (const auto slot = providerSlot(active)) {
        std::lock_guard operation(slot->operations);
        return slot->provider->status();
    }
    return "Provider not found";
}

bool RemoteAccessManager::activateRoute(const std::string& providerId,
                                        const RemoteRouteTarget& target) {
    const auto selected = providerSlot(providerId);
    if (!selected || target.peerId.empty())
        return false;

    const bool exclusive = selected->provider->routesAreExclusive();
    const RouteKey key{providerId, target.peerId};
    std::shared_ptr<RouteEntry> entry;
    std::vector<RemoteRouteTarget> retiredTargets;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (activeProviderId_ != providerId)
            return false;

        const auto existing = activeRoutes_.find(key);
        if (existing != activeRoutes_.end()) {
            entry = existing->second;
            entry->changed.wait(lock, [&entry] {
                return entry->state != RouteState::Activating;
            });
            if (entry->state != RouteState::Active ||
                entry->generation != providerGeneration_)
                return false;
            ++entry->references;
            return true;
        }

        entry = std::make_shared<RouteEntry>();
        entry->target = target;
        entry->generation = providerGeneration_;
        activeRoutes_.emplace(key, entry);

        if (exclusive) {
            for (auto route = activeRoutes_.begin();
                 route != activeRoutes_.end();) {
                if (route->first.providerId == providerId &&
                    !(route->first == key)) {
                    if (route->second->state == RouteState::Active)
                        retiredTargets.push_back(route->second->target);
                    route->second->state = RouteState::Failed;
                    route->second->changed.notify_all();
                    route = activeRoutes_.erase(route);
                } else {
                    ++route;
                }
            }
        }
    }

    std::lock_guard operation(selected->operations);
    for (const auto& retired : retiredTargets)
        selected->provider->deactivateRoute(retired);
    const bool activated = selected->provider->activateRoute(target);
    bool staleActivation = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto current = activeRoutes_.find(key);
        const bool generationMatches =
            activeProviderId_ == providerId &&
            entry->generation == providerGeneration_ &&
            current != activeRoutes_.end() && current->second == entry;

        if (!activated || !generationMatches) {
            entry->state = RouteState::Failed;
            if (current != activeRoutes_.end() && current->second == entry)
                activeRoutes_.erase(current);
            staleActivation = activated;
        } else {
            entry->state = RouteState::Active;
        }
        entry->changed.notify_all();
    }

    if (staleActivation)
        selected->provider->deactivateRoute(target);
    return activated && !staleActivation;
}

bool RemoteAccessManager::prepareRouteForStreaming(
    const std::string& providerId, const std::string& peerId) {
    std::shared_ptr<ProviderSlot> selected;
    RemoteRouteTarget target;
    std::uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const RouteKey key{providerId, peerId};
        const auto route = activeRoutes_.find(key);
        if (route == activeRoutes_.end() ||
            route->second->state != RouteState::Active)
            return false;
        target = route->second->target;
        generation = route->second->generation;
        const auto providerIt = std::find_if(
            providers_.begin(), providers_.end(),
            [&providerId](const auto& slot) {
                return slot->provider->id() == providerId;
            });
        if (providerIt == providers_.end())
            return false;
        selected = *providerIt;
    }
    std::lock_guard operation(selected->operations);
    const bool prepared = selected->provider->prepareRouteForStreaming(target);
    std::lock_guard<std::mutex> lock(mutex_);
    return prepared && activeProviderId_ == providerId &&
           generation == providerGeneration_;
}

void RemoteAccessManager::deactivateRoute(const std::string& providerId, const std::string& peerId) {
    std::shared_ptr<ProviderSlot> selected;
    RemoteRouteTarget target;
    bool shouldDeactivate = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const RouteKey key{providerId, peerId};
        const auto route = activeRoutes_.find(key);
        if (route == activeRoutes_.end() ||
            route->second->state != RouteState::Active)
            return;
        if (route->second->references > 1) {
            --route->second->references;
            return;
        }
        target = route->second->target;
        const auto providerIt = std::find_if(
            providers_.begin(), providers_.end(),
            [&providerId](const auto& slot) {
                return slot->provider->id() == providerId;
            });
        if (providerIt != providers_.end())
            selected = *providerIt;
        activeRoutes_.erase(route);
        shouldDeactivate = activeProviderId_ == providerId;
    }
    if (shouldDeactivate && selected) {
        std::lock_guard operation(selected->operations);
        selected->provider->deactivateRoute(target);
    }
}
