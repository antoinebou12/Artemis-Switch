#pragma once
#include "IRemoteAccessProvider.hpp"
#include "RemoteAccessManager.hpp"
#include <string>

class RemoteRouteLease {
public:
    RemoteRouteLease() noexcept = default;

    RemoteRouteLease(RemoteAccessManager& mgr, const std::string& providerId,
                     RemoteRouteTarget target)
        : mgr_(&mgr), providerId_(providerId), peerId_(target.peerId),
          targetAddress_(target.targetAddress),
          connectAddress_(target.connectAddress), active_(false),
          refused_(false)
    {
        if (!providerId_.empty()) {
            active_ = mgr_->activateRoute(providerId_, target);
            refused_ = !active_;
        } else {
            active_ = true;
        }
    }

    ~RemoteRouteLease() {
        release();
    }

    RemoteRouteLease(const RemoteRouteLease&) = delete;
    RemoteRouteLease& operator=(const RemoteRouteLease&) = delete;

    RemoteRouteLease(RemoteRouteLease&& other) noexcept
        : mgr_(other.mgr_), providerId_(std::move(other.providerId_)), peerId_(std::move(other.peerId_)),
          targetAddress_(std::move(other.targetAddress_)), connectAddress_(std::move(other.connectAddress_)), active_(other.active_),
          refused_(other.refused_)
    {
        other.mgr_ = nullptr;
        other.active_ = false;
        other.refused_ = false;
    }

    RemoteRouteLease& operator=(RemoteRouteLease&& other) noexcept {
        if (this != &other) {
            release();
            mgr_ = other.mgr_;
            providerId_ = std::move(other.providerId_);
            peerId_ = std::move(other.peerId_);
            targetAddress_ = std::move(other.targetAddress_);
            connectAddress_ = std::move(other.connectAddress_);
            active_ = other.active_;
            refused_ = other.refused_;
            other.mgr_ = nullptr;
            other.active_ = false;
            other.refused_ = false;
        }
        return *this;
    }

    bool isActive() const noexcept { return active_; }
    // True when the address named a known tunnel peer but the provider
    // refused the route (no relay, no proxy). Callers must fail fast with the
    // provider's reason instead of dialing the overlay address directly,
    // which can only blackhole into a minute-long TCP timeout.
    bool refused() const noexcept { return refused_; }
    const std::string& providerId() const noexcept { return providerId_; }
    const std::string& peerId() const noexcept { return peerId_; }
    const std::string& targetAddress() const noexcept { return targetAddress_; }
    const std::string& connectAddress() const noexcept { return connectAddress_; }

    bool prepareForStreaming() {
        return active_ && mgr_ &&
               (providerId_.empty() ||
                mgr_->prepareRouteForStreaming(providerId_, peerId_));
    }

    void release() noexcept {
        if (active_ && mgr_ && !providerId_.empty()) {
            mgr_->deactivateRoute(providerId_, peerId_);
            active_ = false;
        }
    }

private:
    RemoteAccessManager* mgr_ = nullptr;
    std::string providerId_;
    std::string peerId_;
    std::string targetAddress_;
    std::string connectAddress_;
    bool active_ = false;
    bool refused_ = false;
};
