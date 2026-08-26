#pragma once
#include "IRemoteAccessProvider.hpp"
#include "RemoteAccessManager.hpp"
#include <string>

class RemoteRouteLease {
public:
    RemoteRouteLease() noexcept = default;

    RemoteRouteLease(RemoteAccessManager& mgr, const std::string& providerId, const std::string& peerId, std::string targetAddress, std::string connectAddress)
        : mgr_(&mgr), providerId_(providerId), peerId_(peerId), targetAddress_(std::move(targetAddress)), connectAddress_(std::move(connectAddress)), active_(false)
    {
        if (!providerId_.empty()) {
            active_ = mgr_->activateRoute(providerId_, peerId_);
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
          targetAddress_(std::move(other.targetAddress_)), connectAddress_(std::move(other.connectAddress_)), active_(other.active_)
    {
        other.mgr_ = nullptr;
        other.active_ = false;
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
            other.mgr_ = nullptr;
            other.active_ = false;
        }
        return *this;
    }

    bool isActive() const noexcept { return active_; }
    const std::string& providerId() const noexcept { return providerId_; }
    const std::string& peerId() const noexcept { return peerId_; }
    const std::string& targetAddress() const noexcept { return targetAddress_; }
    const std::string& connectAddress() const noexcept { return connectAddress_; }

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
};
