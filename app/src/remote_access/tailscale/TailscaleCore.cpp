#include "TailscaleCore.hpp"

#include <chrono>
#include <utility>

namespace artemis::tailscale {

TailscaleCore::TailscaleCore(std::filesystem::path statePath,
                             std::unique_ptr<IControlSession> control,
                             std::unique_ptr<IOverlayRoute> overlay,
                             IdentityGenerator identityGenerator)
    : stateStore_(std::move(statePath)), control_(std::move(control)),
      overlay_(std::move(overlay)),
      identityGenerator_(std::move(identityGenerator)) {}

TailscaleCore::~TailscaleCore() { stop(); }

bool TailscaleCore::start(SecureBytes authKey, SecureBytes passphrase) {
    if (worker_.joinable())
        return false;
    stopRequested_ = false;
    setState(Snapshot::State::Starting, "Starting");
    worker_ = std::thread(&TailscaleCore::workerMain, this, std::move(authKey),
                          std::move(passphrase));
    return true;
}

void TailscaleCore::stop() noexcept {
    stopRequested_ = true;
    if (control_)
        control_->close();
    if (worker_.joinable())
        worker_.join();
    {
        std::lock_guard lock(routeMutex_);
        if (overlay_)
            overlay_->stop();
        activeRoute_.reset();
    }
    setState(Snapshot::State::Stopped, "Stopped");
}

Snapshot TailscaleCore::snapshot() const {
    std::lock_guard lock(snapshotMutex_);
    auto copy = snapshot_;
    copy.peers = peers_.snapshot();
    return copy;
}

std::optional<RemoteRouteTarget> TailscaleCore::resolveRoute(
    std::string_view address) const {
    return peers_.resolveIPv4(address);
}

bool TailscaleCore::activateRoute(const RemoteRouteTarget& target) {
    std::lock_guard lock(routeMutex_);
    if (!overlay_)
        return false;
    if (activeRoute_ && activeRoute_->peerId == target.peerId)
        return true;
    overlay_->stop();
    std::string error;
    if (!overlay_->start(target, &error)) {
        setState(Snapshot::State::Error, "Route failed", std::move(error));
        activeRoute_.reset();
        return false;
    }
    activeRoute_ = target;
    return true;
}

bool TailscaleCore::prepareRouteForStreaming(
    const RemoteRouteTarget& target) {
    std::lock_guard lock(routeMutex_);
    if (!overlay_ || !activeRoute_ || activeRoute_->peerId != target.peerId)
        return false;
    std::string error;
    if (!overlay_->prepareForStreaming(target, &error)) {
        setState(Snapshot::State::Error, "Stream route failed",
                 std::move(error));
        return false;
    }
    return true;
}

void TailscaleCore::deactivateRoute(const RemoteRouteTarget& target) noexcept {
    std::lock_guard lock(routeMutex_);
    if (overlay_ && activeRoute_ && activeRoute_->peerId == target.peerId) {
        overlay_->stop();
        activeRoute_.reset();
    }
}

RemotePathInfo TailscaleCore::pathInfo(std::string_view peerId) const {
    return paths_.pathInfo(peerId);
}

bool TailscaleCore::replacePeers(std::vector<Peer> peers,
                                 std::string localAddress,
                                 std::string* error) {
    if (!PeerDirectory::isLiteralIPv4(localAddress)) {
        if (error) *error = "control map has no usable IPv4 address";
        return false;
    }
    if (!peers_.replace(std::move(peers), error))
        return false;
    {
        std::lock_guard lock(snapshotMutex_);
        snapshot_.localAddress = std::move(localAddress);
        snapshot_.state = Snapshot::State::Ready;
        snapshot_.status = "Ready";
        snapshot_.lastError.clear();
    }
    return true;
}

bool TailscaleCore::applyPeerDelta(const PeerDelta& delta,
                                   std::string* error) {
    return peers_.apply(delta, error);
}

void TailscaleCore::workerMain(SecureBytes authKey, SecureBytes passphrase) {
    std::string error;
    auto identity = stateStore_.load(passphrase.view(), &error);
    if (!identity) {
        if (error != "Tailscale state does not exist") {
            setState(Snapshot::State::Error, "Identity unavailable", error);
            return;
        }
        Identity generated;
        if (!identityGenerator_ || !identityGenerator_(generated, &error)) {
            setState(Snapshot::State::Error, "Identity generation failed",
                     error);
            return;
        }
        const auto protection = passphrase.view().empty()
                                    ? StateProtection::Plain
                                    : StateProtection::Passphrase;
        if (!stateStore_.save(generated, protection, passphrase.view(), {},
                              &error)) {
            setState(Snapshot::State::Error, "Identity save failed", error);
            return;
        }
        identity = generated;
    }
    passphrase.clear();

    if (!control_) {
        setState(Snapshot::State::Error, "Control unavailable",
                 "Tailscale control transport is not linked");
        return;
    }
    setState(Snapshot::State::ConnectingControl, "Connecting control");
    const bool hadAuthKey = !authKey.view().empty();
    if (!control_->connect(*identity, authKey.view(), &error)) {
        authKey.clear();
        setState(hadAuthKey ? Snapshot::State::Error
                            : Snapshot::State::NeedsAuthentication,
                 "Authentication failed", error);
        return;
    }
    authKey.clear();
    setState(Snapshot::State::ConnectedControl, "Control connected");

    while (!stopRequested_) {
        PeerDelta delta;
        std::optional<std::vector<Peer>> fullPeers;
        std::string localAddress;
        if (!control_->poll(&delta, &fullPeers, &localAddress, &error)) {
            if (!stopRequested_)
                setState(Snapshot::State::Error, "Control disconnected", error);
            break;
        }
        if (fullPeers) {
            if (!replacePeers(std::move(*fullPeers), localAddress, &error)) {
                if (!stopRequested_)
                    setState(Snapshot::State::Error, "Netmap rejected", error);
                break;
            }
            continue;
        }
        if (!delta.changed.empty() || !delta.removedStableIds.empty())
            peers_.apply(delta, &error);
        if (!localAddress.empty()) {
            std::lock_guard lock(snapshotMutex_);
            snapshot_.localAddress = std::move(localAddress);
            snapshot_.state = Snapshot::State::Ready;
            snapshot_.status = "Ready";
            snapshot_.lastError.clear();
        }
        paths_.poll(PathManager::Clock::now());
    }
    control_->close();
}

void TailscaleCore::setState(Snapshot::State state, std::string status,
                             std::string error) {
    std::lock_guard lock(snapshotMutex_);
    snapshot_.state = state;
    snapshot_.status = std::move(status);
    snapshot_.lastError = std::move(error);
}

} // namespace artemis::tailscale
