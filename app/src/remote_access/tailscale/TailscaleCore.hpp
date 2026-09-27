#pragma once

#include "TailscalePathManager.hpp"
#include "TailscalePeerDirectory.hpp"
#include "TailscaleStateStore.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace artemis::tailscale {

class IControlSession {
public:
    virtual ~IControlSession() = default;
    virtual bool connect(const Identity& identity,
                         std::span<const std::uint8_t> authKey,
                         std::string* error) = 0;
    // Returns the incremental change since the previous poll. When the control
    // stream begins with a full netmap, `fullPeers` is populated instead and
    // `delta` is left empty; the engine replaces its peer set wholesale so
    // peers that left the tailnet are really removed rather than lingering.
    virtual bool poll(PeerDelta* delta,
                      std::optional<std::vector<Peer>>* fullPeers,
                      std::string* localAddress,
                      std::optional<std::vector<DerpRegion>>* derpMap,
                      std::string* error) = 0;
    // Tells control which DERP region is this node's home, so peers know
    // where to send packets for us. Called on the poll thread between polls.
    // The default (for sessions without a live control plane) is a no-op.
    virtual bool sendHostinfoUpdate(int preferredDerp, std::string* error) {
        (void)preferredDerp;
        (void)error;
        return true;
    }
    // Home DERP region to put in the next connect()'s initial MapRequest
    // (Hostinfo.NetInfo.PreferredDERP). 0 omits it.
    virtual void setInitialPreferredDerp(int region) noexcept { (void)region; }
    virtual void close() noexcept = 0;
    // Called from stop() on another thread: unblocks a poll() waiting on the
    // network so the worker can exit and close() the session itself.
    virtual void interrupt() noexcept {}
};

class IOverlayRoute {
public:
    virtual ~IOverlayRoute() = default;
    virtual bool start(const RemoteRouteTarget& target,
                       std::string* error) = 0;
    virtual bool prepareForStreaming(const RemoteRouteTarget& target,
                                     std::string* error) = 0;
    virtual void stop() noexcept = 0;
};

class TailscaleCore {
public:
    using IdentityGenerator = std::function<bool(Identity&, std::string*)>;

    TailscaleCore(std::filesystem::path statePath,
                  std::unique_ptr<IControlSession> control,
                  std::unique_ptr<IOverlayRoute> overlay,
                  IdentityGenerator identityGenerator);
    ~TailscaleCore();

    TailscaleCore(const TailscaleCore&) = delete;
    TailscaleCore& operator=(const TailscaleCore&) = delete;

    bool start(SecureBytes authKey, SecureBytes passphrase);
    void stop() noexcept;
    [[nodiscard]] Snapshot snapshot() const;
    [[nodiscard]] std::optional<RemoteRouteTarget> resolveRoute(
        std::string_view address) const;
    bool activateRoute(const RemoteRouteTarget& target);
    bool prepareRouteForStreaming(const RemoteRouteTarget& target);
    void deactivateRoute(const RemoteRouteTarget& target) noexcept;
    [[nodiscard]] RemotePathInfo pathInfo(std::string_view peerId) const;
    [[nodiscard]] std::optional<Identity> identity() const;

    // Portable integration seam for decoded full maps and incremental updates.
    bool replacePeers(std::vector<Peer> peers, std::string localAddress,
                      std::string* error = nullptr);
    bool applyPeerDelta(const PeerDelta& delta, std::string* error = nullptr);
    // Stores the latest control-plane DERP region map. Only frames that carry
    // a DERPMap section update it; deltas leave the stored map untouched.
    void updateDerpMap(std::vector<DerpRegion> regions);

private:
    void workerMain(SecureBytes authKey, SecureBytes passphrase);
    // Advertises `region` as this node's home DERP region (Hostinfo.NetInfo
    // .PreferredDERP) if it differs from what was last sent. Peers route
    // their packets for us to that region, so it must match the region the
    // relay is connected to.
    void advertiseHomeDerp(int region, const char* reason);
    // One control session: connect, then poll until it fails or stop() is
    // requested. The DERP relay and WireGuard route are independent of it
    // and keep running while control reconnects.
    enum class SessionResult {
        Stopped,         // stop() was requested
        ConnectFailed,   // connect() failed
        DroppedEarly,    // connected, then failed before any message
        DroppedAfterData, // worked for a while, then the stream dropped
        Fatal,           // control sent data this client rejects
    };
    SessionResult runControlSession(SecureBytes& authKey,
                                    const Identity& identity,
                                    bool firstSession);
    // Sleeps up to `delay`, returning early (false) when stop() is requested.
    bool waitBeforeReconnect(std::chrono::seconds delay);
    int loadHomeDerpHint() const;
    void saveHomeDerpHint(int region) const;
    // Default home region before any route exists: the most common home
    // region of online peers, else the lowest region in the DERP map.
    int chooseDefaultHomeDerp() const;
    void setState(Snapshot::State state, std::string status,
                  std::string error = {});

    StateStore stateStore_;
    std::unique_ptr<IControlSession> control_;
    std::unique_ptr<IOverlayRoute> overlay_;
    IdentityGenerator identityGenerator_;
    PeerDirectory peers_;
    PathManager paths_;

    mutable std::mutex identityMutex_;
    std::optional<Identity> identity_;
    mutable std::mutex snapshotMutex_;
    Snapshot snapshot_;
    std::mutex routeMutex_;
    std::optional<RemoteRouteTarget> activeRoute_;
    std::atomic_bool stopRequested_{false};
    std::atomic_bool controlConnected_{false};
    std::mutex advertiseMutex_;
    int advertisedDerp_ = 0; // guarded by advertiseMutex_
    // Region to advertise on the next (re)connect: the last one advertised
    // or requested by a route, even while control was down.
    int homeDerpHint_ = 0; // guarded by advertiseMutex_
    // Small non-secret sidecar next to the identity state, so the very first
    // MapRequest after an app restart already carries PreferredDERP.
    std::filesystem::path homeDerpPath_;
    std::mutex stopMutex_;
    std::condition_variable stopSignal_;
    std::thread worker_;
};

} // namespace artemis::tailscale
