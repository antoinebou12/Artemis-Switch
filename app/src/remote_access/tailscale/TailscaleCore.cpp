
#if defined(__SWITCH__)
#include "../../utils/Settings.hpp"
#include "../../vpn/VpnFileLogger.hpp"
namespace {
void logTsCore(VpnFileLogger::Severity severity, std::string_view message) {
    VpnFileLogger::append(Settings::instance().working_dir() + "/vpn.log",
                          "TS", severity, message);
}
}
#define LOG_CORE_INFO(msg) logTsCore(VpnFileLogger::Severity::Info, msg)
#define LOG_CORE_WARN(msg) logTsCore(VpnFileLogger::Severity::Warning, msg)
#define LOG_CORE_ERROR(msg) logTsCore(VpnFileLogger::Severity::Error, msg)
#else
#define LOG_CORE_INFO(msg) do { (void)(msg); } while(0)
#define LOG_CORE_WARN(msg) do { (void)(msg); } while(0)
#define LOG_CORE_ERROR(msg) do { (void)(msg); } while(0)
#endif
#include "TailscaleWgxRoute.hpp"
#include "TailscaleCore.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace artemis::tailscale {

TailscaleCore::TailscaleCore(std::filesystem::path statePath,
                             std::unique_ptr<IControlSession> control,
                             std::unique_ptr<IOverlayRoute> overlay,
                             IdentityGenerator identityGenerator)
    : stateStore_(std::move(statePath)), control_(std::move(control)),
      overlay_(std::move(overlay)),
      identityGenerator_(std::move(identityGenerator)) {
    if (auto* wgxRoute = dynamic_cast<TailscaleWgxRoute*>(overlay_.get())) {
        wgxRoute->setPeerResolver([this](std::string_view peerId) {
            return peers_.findByStableId(peerId);
        });
        wgxRoute->setLocalInfoProvider([this]() -> std::optional<std::pair<std::string, Key32>> {
            std::lock_guard lock(snapshotMutex_);
            if (snapshot_.localAddress.empty())
                return std::nullopt;
            std::lock_guard idLock(identityMutex_);
            if (!identity_)
                return std::nullopt;
            return std::make_pair(snapshot_.localAddress, identity_->nodePrivate);
        });
        wgxRoute->setDerpMapProvider([this]() {
            std::lock_guard lock(snapshotMutex_);
            return snapshot_.derpMap;
        });
    }
}

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
    // Wake the worker out of its blocking control read. Closing the session
    // from here instead raced the worker and, on Switch, sslConnectionClose
    // waited for the pending read, so Disconnect hung until the control
    // server's next keepalive (~30 s). The worker closes the session itself.
    if (control_)
        control_->interrupt();
    if (worker_.joinable())
        worker_.join();
    if (control_)
        control_->close();
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
    // A subnet router peer can front several LAN hosts, so the route is only
    // reusable when it also dials the same host.
    if (activeRoute_ && activeRoute_->peerId == target.peerId &&
        activeRoute_->targetAddress == target.targetAddress) {
        LOG_CORE_INFO("route already active for peer " + target.peerId);
        return true;
    }
    LOG_CORE_INFO("route activation begin: peer=" + target.peerId +
                  " addr=" + target.peerAddress +
                  (target.targetAddress.empty() ||
                           target.targetAddress == target.peerAddress
                       ? std::string{}
                       : " host=" + target.targetAddress + " (subnet route)"));
    const auto started = std::chrono::steady_clock::now();
    // The relay connects to the peer's home region; advertise that same
    // region as ours so the peer's replies are sent where we are listening.
    if (const auto peer = peers_.findByStableId(target.peerId);
        peer && peer->homeDerp > 0)
        advertiseHomeDerp(peer->homeDerp, "route target's home region");
    overlay_->stop();
    std::string error;
    const bool ok = overlay_->start(target, &error);
    const auto elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started)
            .count();
    if (!ok) {
        LOG_CORE_ERROR("route activation failed after " +
                       std::to_string(elapsedMs) + " ms: " + error);
        setState(Snapshot::State::Error, "Route failed", std::move(error));
        activeRoute_.reset();
        return false;
    }
    LOG_CORE_INFO("route activation succeeded in " + std::to_string(elapsedMs) +
                  " ms: GameStream traffic to " +
                  (target.targetAddress.empty() ? target.peerAddress
                                                : target.targetAddress) +
                  " now goes through the tunnel");
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
        LOG_CORE_ERROR("stream UDP relays failed: " + error);
        setState(Snapshot::State::Error, "Stream route failed",
                 std::move(error));
        return false;
    }
    LOG_CORE_INFO("stream UDP relays ready for " + target.peerAddress);
    return true;
}

void TailscaleCore::deactivateRoute(const RemoteRouteTarget& target) noexcept {
    std::lock_guard lock(routeMutex_);
    if (overlay_ && activeRoute_ && activeRoute_->peerId == target.peerId &&
        activeRoute_->targetAddress == target.targetAddress) {
        LOG_CORE_INFO("route released for peer " + target.peerId);
        overlay_->stop();
        activeRoute_.reset();
    }
}

std::optional<Identity> TailscaleCore::identity() const {
    std::lock_guard lock(identityMutex_);
    return identity_;
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

void TailscaleCore::updateDerpMap(std::vector<DerpRegion> regions) {
    std::lock_guard lock(snapshotMutex_);
    snapshot_.derpMap = std::move(regions);
}

void TailscaleCore::workerMain(SecureBytes authKey, SecureBytes passphrase) {
    std::string error;
    auto identity = stateStore_.load(passphrase.view(), &error);
    bool replacingWeakIdentity = false;
    if (identity && isWeakIdentity(*identity)) {
        LOG_CORE_ERROR(
            "saved identity is unsafe: its machine, node and disco keys are "
            "identical (older builds used a broken random source, so these "
            "keys are predictable). Generating a new identity; this needs a "
            "fresh login key, and the old device should be removed in the "
            "admin console");
        identity.reset();
        replacingWeakIdentity = true;
    } else if (identity) {
        LOG_CORE_INFO("identity loaded from saved state (same nodekey as "
                      "the previous run)");
    }
    if (!identity) {
        if (!replacingWeakIdentity &&
            error != "Tailscale state does not exist") {
            LOG_CORE_ERROR("saved identity could not be loaded: " + error);
            setState(Snapshot::State::Error, "Identity unavailable", error);
            return;
        }
        LOG_CORE_INFO("no saved identity; generating a new node identity "
                      "(this needs a fresh login key to register)");
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
    {
        std::lock_guard lock(identityMutex_);
        identity_ = identity;
    }
    passphrase.clear();

    if (!control_) {
        setState(Snapshot::State::Error, "Control unavailable",
                 "Tailscale control transport is not linked");
        return;
    }
    setState(Snapshot::State::ConnectingControl, "Connecting control");
    LOG_CORE_INFO("Attempting control connection...");
    const bool hadAuthKey = !authKey.view().empty();
    if (!control_->connect(*identity, authKey.view(), &error)) {
        LOG_CORE_ERROR("control connect failed: " + error);
        authKey.clear();
        setState(hadAuthKey ? Snapshot::State::Error
                            : Snapshot::State::NeedsAuthentication,
                 "Authentication failed", error);
        return;
    }
    authKey.clear();
    setState(Snapshot::State::ConnectedControl, "Control connected");
    LOG_CORE_INFO("control connect succeeded. Polling netmap stream...");
    controlConnected_ = true;
    {
        std::lock_guard lock(advertiseMutex_);
        advertisedDerp_ = 0; // a new map session starts with no NetInfo
    }

    // Until a route picks a region, advertise a sensible default so this node
    // never sits at HomeDERP=0 (peers cannot reply to a node without one).
    const auto advertiseDefault = [this] {
        {
            std::lock_guard lock(advertiseMutex_);
            if (advertisedDerp_ != 0)
                return;
        }
        advertiseHomeDerp(chooseDefaultHomeDerp(), "default");
    };
    const auto sessionStart = std::chrono::steady_clock::now();
    auto lastMessage = sessionStart;
    std::uint64_t messageCount = 0;
    while (!stopRequested_) {
        PeerDelta delta;
        std::optional<std::vector<Peer>> fullPeers;
        std::string localAddress;
        std::optional<std::vector<DerpRegion>> derpMap;
        if (!control_->poll(&delta, &fullPeers, &localAddress, &derpMap, &error)) {
            const auto now = std::chrono::steady_clock::now();
            const auto sinceLast =
                std::chrono::duration_cast<std::chrono::seconds>(now - lastMessage)
                    .count();
            const auto sessionAge =
                std::chrono::duration_cast<std::chrono::seconds>(now - sessionStart)
                    .count();
            LOG_CORE_ERROR("control poll failed: " + error + " (after " +
                           std::to_string(messageCount) + " messages, " +
                           std::to_string(sessionAge) + " s connected, " +
                           std::to_string(sinceLast) +
                           " s since the last control message)");
            if (sinceLast > 90)
                LOG_CORE_WARN(
                    "control was silent for more than 90 s before the drop: "
                    "control normally sends a keepalive about every minute, "
                    "so the stream had stalled (HTTP/2 flow control or a "
                    "dead network path) before it failed");
            if (!stopRequested_)
                setState(Snapshot::State::Error, "Control disconnected", error);
            break;
        }
        lastMessage = std::chrono::steady_clock::now();
        ++messageCount;
        // One summary line per poll so a device log shows exactly what each
        // frame carried: full peer lists, deltas, relay maps, or nothing.
        {
            std::string summary = "netmap update: full=";
            summary += fullPeers ? std::to_string(fullPeers->size()) : "none";
            summary += " local=";
            summary += localAddress.empty() ? "none" : localAddress;
            summary += " derp=";
            summary += derpMap ? std::to_string(derpMap->size()) + " regions"
                               : "none";
            summary += " delta=+" + std::to_string(delta.changed.size()) + "/-" +
                       std::to_string(delta.removedStableIds.size()) + "/~" +
                       std::to_string(delta.onlineChanges.size());
            LOG_CORE_INFO(summary);
        }
        if (!localAddress.empty()) {
            LOG_CORE_INFO("Assigned local VPN address: " + localAddress);
        }
        if (fullPeers) {
            LOG_CORE_INFO("Received full netmap with " + std::to_string(fullPeers->size()) + " peers");
            if (!replacePeers(std::move(*fullPeers), localAddress, &error)) {
                if (!stopRequested_)
                    setState(Snapshot::State::Error, "Netmap rejected", error);
                break;
            }
            if (derpMap) {
                LOG_CORE_INFO("Stored DERP region map (" +
                              std::to_string(derpMap->size()) + " regions)");
                updateDerpMap(std::move(*derpMap));
            }
            advertiseDefault();
            continue;
        }
        if (!delta.changed.empty() || !delta.removedStableIds.empty() ||
            !delta.onlineChanges.empty())
            peers_.apply(delta, &error);
        if (derpMap) {
            LOG_CORE_INFO("Stored DERP region map (" +
                          std::to_string(derpMap->size()) + " regions)");
            updateDerpMap(std::move(*derpMap));
        }
        if (!localAddress.empty()) {
            std::lock_guard lock(snapshotMutex_);
            snapshot_.localAddress = std::move(localAddress);
            snapshot_.state = Snapshot::State::Ready;
            snapshot_.status = "Ready";
            snapshot_.lastError.clear();
        }
        paths_.poll(PathManager::Clock::now());
        advertiseDefault();
    }
    controlConnected_ = false;
    control_->close();
}

void TailscaleCore::advertiseHomeDerp(int region, const char* reason) {
    if (region <= 0 || !control_ || !controlConnected_)
        return;
    std::lock_guard lock(advertiseMutex_);
    if (region == advertisedDerp_)
        return;
    std::string error;
    if (!control_->sendHostinfoUpdate(region, &error)) {
        LOG_CORE_ERROR("could not advertise home DERP region " +
                       std::to_string(region) + ": " + error);
        return;
    }
    LOG_CORE_INFO("advertised home DERP region " + std::to_string(region) +
                  " (" + reason + ", previously " +
                  std::to_string(advertisedDerp_) +
                  "); peers learn it with their next netmap update");
    advertisedDerp_ = region;
}

int TailscaleCore::chooseDefaultHomeDerp() const {
    std::vector<DerpRegion> regions;
    {
        std::lock_guard lock(snapshotMutex_);
        regions = snapshot_.derpMap;
    }
    if (regions.empty())
        return 0;
    std::unordered_map<int, int> votes;
    for (const auto& peer : peers_.snapshot()) {
        if (peer.online && peer.homeDerp > 0)
            ++votes[peer.homeDerp];
    }
    int best = 0;
    int bestVotes = 0;
    for (const auto& [region, count] : votes) {
        if (count > bestVotes || (count == bestVotes && region < best)) {
            best = region;
            bestVotes = count;
        }
    }
    if (best > 0)
        return best;
    int lowest = 0;
    for (const auto& region : regions) {
        if (!region.nodes.empty() && (lowest == 0 || region.regionId < lowest))
            lowest = region.regionId;
    }
    return lowest;
}

void TailscaleCore::setState(Snapshot::State state, std::string status,
                             std::string error) {
    std::lock_guard lock(snapshotMutex_);
    snapshot_.state = state;
    snapshot_.status = std::move(status);
    snapshot_.lastError = std::move(error);
}

} // namespace artemis::tailscale
