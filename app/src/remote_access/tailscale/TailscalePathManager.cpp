#include "TailscalePathManager.hpp"

namespace artemis::tailscale {

void PathManager::setDerp(std::string_view peerId, std::string region) {
    std::lock_guard lock(mutex_);
    paths_[std::string(peerId)].derpRegion = std::move(region);
}

void PathManager::beginDirectProbe(std::string_view peerId,
                                   std::string endpoint,
                                   Clock::time_point now) {
    std::lock_guard lock(mutex_);
    auto& state = paths_[std::string(peerId)];
    state.directEndpoint = std::move(endpoint);
    state.probeStarted = now;
    state.probing = true;
}

void PathManager::directPong(std::string_view peerId, std::string endpoint,
                             int rttMs, Clock::time_point now) {
    std::lock_guard lock(mutex_);
    auto& state = paths_[std::string(peerId)];
    state.directEndpoint = std::move(endpoint);
    state.directRttMs = rttMs;
    state.lastDirectPacket = now;
    state.probing = false;
    state.direct = true;
}

void PathManager::noteDirectPacket(std::string_view peerId,
                                   Clock::time_point now) {
    std::lock_guard lock(mutex_);
    const auto found = paths_.find(std::string(peerId));
    if (found != paths_.end() && found->second.direct)
        found->second.lastDirectPacket = now;
}

void PathManager::networkChanged() {
    std::lock_guard lock(mutex_);
    for (auto& [_, state] : paths_) {
        state.direct = false;
        state.probing = false;
        state.directEndpoint.clear();
        state.directRttMs = -1;
    }
}

void PathManager::poll(Clock::time_point now) {
    std::lock_guard lock(mutex_);
    for (auto& [_, state] : paths_) {
        if (state.probing && now - state.probeStarted > kProbeTimeout)
            state.probing = false;
        if (state.direct && now - state.lastDirectPacket > kDirectFreshness) {
            state.direct = false;
            state.directRttMs = -1;
        }
    }
}

RemotePathInfo PathManager::pathInfo(std::string_view peerId) const {
    std::lock_guard lock(mutex_);
    const auto found = paths_.find(std::string(peerId));
    if (found == paths_.end())
        return {};
    const auto& state = found->second;
    if (state.direct) {
        return {RemotePathType::DirectIPv4, state.directEndpoint, {},
                state.directRttMs};
    }
    if (!state.derpRegion.empty()) {
        return {RemotePathType::Derp, {}, state.derpRegion, -1};
    }
    return {};
}

} // namespace artemis::tailscale
