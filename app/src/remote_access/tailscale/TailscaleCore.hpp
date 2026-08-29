#pragma once

#include "TailscalePathManager.hpp"
#include "TailscalePeerDirectory.hpp"
#include "TailscaleStateStore.hpp"

#include <atomic>
#include <condition_variable>
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
                      std::string* localAddress, std::string* error) = 0;
    virtual void close() noexcept = 0;
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

    // Portable integration seam for decoded full maps and incremental updates.
    bool replacePeers(std::vector<Peer> peers, std::string localAddress,
                      std::string* error = nullptr);
    bool applyPeerDelta(const PeerDelta& delta, std::string* error = nullptr);

private:
    void workerMain(SecureBytes authKey, SecureBytes passphrase);
    void setState(Snapshot::State state, std::string status,
                  std::string error = {});

    StateStore stateStore_;
    std::unique_ptr<IControlSession> control_;
    std::unique_ptr<IOverlayRoute> overlay_;
    IdentityGenerator identityGenerator_;
    PeerDirectory peers_;
    PathManager paths_;

    mutable std::mutex snapshotMutex_;
    Snapshot snapshot_;
    std::mutex routeMutex_;
    std::optional<RemoteRouteTarget> activeRoute_;
    std::atomic_bool stopRequested_{false};
    std::thread worker_;
};

} // namespace artemis::tailscale
