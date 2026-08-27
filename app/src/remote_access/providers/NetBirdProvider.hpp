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
// Threading: start(), stop() and refreshPeers() all block (login, thread joins,
// per-peer network probes) and MUST be called off the UI thread. peers() and
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
    bool activateRoute(const std::string& peerId) override;
    bool routesAreExclusive() const override { return true; }
    bool prepareRouteForStreaming(const std::string& peerId) override;
    void deactivateRoute(const std::string& peerId) override;

    // BLOCKING: probes every peer's GameStream port. Call from brls::async.
    void refreshPeers() override;

    // Cache read. True when the address was returned by an authenticated peer
    // sync, which is what gates routing traffic into the tunnel.
    [[nodiscard]] bool isKnownPeer(const std::string& address) const;

private:
    void startPump();
    void stopPump();

    std::string lastError_;
    std::string activePeer_;
    bool started_ = false;
    bool udpRelaysStarted_ = false;

    // NetBird's lwIP/WireGuard timers must continue while the UI thread is in
    // a blocking GameStream request. The atomic in netbird_poll() coalesces
    // legacy socket-level poll calls with this single persistent owner.
    std::atomic<bool> pumpRunning_{false};
    std::atomic<std::uint64_t> pumpPollCount_{0};
    std::atomic<std::uint64_t> pumpLastPollMs_{0};
    std::atomic<std::uint64_t> pumpMaxGapMs_{0};
    std::thread pumpThread_;

    // peers() is read from the UI thread while refreshPeers() runs on a worker.
    mutable std::mutex peersMutex_;
    std::vector<RemoteAccessPeer> cachedPeers_;
};
