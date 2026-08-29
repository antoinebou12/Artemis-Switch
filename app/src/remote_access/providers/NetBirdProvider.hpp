#pragma once
#include "../IRemoteAccessProvider.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>

// Thin wrapper around libnetbird.a. Everything protocol-related -- setup-key
// login, peer sync, relay transport, WireGuard handshake -- lives in that
// library; this class only drives its lifecycle and exposes it to Artemis.
//
// Threading: start() and stop() block (login and thread joins) and MUST be
// called off the UI thread. peers() and
// isKnownPeer() are cheap cache reads and are safe anywhere.
class NetBirdProvider : public IRemoteAccessProvider {
public:
    ~NetBirdProvider() override;

    std::string id() const override;
    std::string name() const override;
    bool available() const override;
    bool start() override;
    void stop() override;
    void poll() override;
    std::string status() const override;
    std::string lastError() const override;
    std::string localAddress() const override;
    std::vector<RemoteAccessPeer> peers() const override;
    std::optional<RemoteRouteTarget>
    resolveRoute(std::string_view address) const override;
    bool activateRoute(const RemoteRouteTarget& target) override;
    bool routesAreExclusive() const override { return true; }
    bool prepareRouteForStreaming(const RemoteRouteTarget& target) override;
    void deactivateRoute(const RemoteRouteTarget& target) override;

    // Refreshes the authenticated management peer cache. Kept on the worker
    // path so future control-plane refreshes cannot stall the UI.
    void refreshPeers() override;

    // Cache read. True when the address was returned by an authenticated peer
    // sync, which is what gates routing traffic into the tunnel.
    [[nodiscard]] bool isKnownPeer(const std::string& address) const;

private:
    struct StateSnapshot {
        std::string lastError;
        std::string activePeer;
        std::string localAddress;
        bool started = false;
        bool ready = false;
        bool udpRelaysStarted = false;
    };

    void startPump();
    void stopPump();
    [[nodiscard]] StateSnapshot stateSnapshot() const;
    void setLastError(std::string error);

    mutable std::mutex stateMutex_;
    StateSnapshot state_;

    std::atomic<bool> pumpRunning_{false};
    std::atomic<std::uint64_t> pumpPollCount_{0};
    std::atomic<std::uint64_t> pumpLastPollMs_{0};
    std::atomic<std::uint64_t> pumpMaxGapMs_{0};
    std::thread pumpThread_;

    // peers() is read from the UI thread while refreshPeers() runs on a worker.
    mutable std::mutex peersMutex_;
    std::vector<RemoteAccessPeer> cachedPeers_;
};
