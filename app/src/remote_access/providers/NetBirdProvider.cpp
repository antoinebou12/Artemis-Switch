#include "NetBirdProvider.hpp"

#include "../../utils/Settings.hpp"
#include "../../vpn/VpnFileLogger.hpp"

#include <borealis/core/logger.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>

#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
extern "C" {
#include <netbird.h>
#include <switch.h>
}
#endif

using namespace brls::literals;

namespace {

// Moonlight's GameStream HTTP port. The TCP proxy is anchored here; the UDP
// relay covers the video/audio/control ports on its own.
constexpr uint16_t kGameStreamPort = 47989;

// The first connection to a peer may also have to establish its WireGuard
// handshake. A 300 ms LAN-style deadline produced false "offline" results on
// relayed peers, so allow one realistic handshake window. Probing stays off the
// UI thread and happens only during an explicit peer refresh.
constexpr int kPeerProbeTimeoutMs = 1500;
constexpr auto kPumpInterval = std::chrono::milliseconds(5);
constexpr auto kPumpWatchdogInterval = std::chrono::seconds(5);

std::uint64_t steady_milliseconds() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

std::string vpn_log_path() {
    return Settings::instance().working_dir() + "/vpn.log";
}

void vpn_log(VpnFileLogger::Severity severity, const std::string& message) {
    VpnFileLogger::append(vpn_log_path(), "NetBird", severity, message);
}

} // namespace

NetBirdProvider::~NetBirdProvider() {
    if (started_) {
        stop();
    } else {
        stopPump();
    }
}

std::string NetBirdProvider::id() const { return "netbird"; }
std::string NetBirdProvider::name() const { return "NetBird"; }

bool NetBirdProvider::available() const {
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    return true;
#else
    return false;
#endif
}

bool NetBirdProvider::start() {
#if !defined(__SWITCH__) || !defined(ENABLE_NETBIRD)
    lastError_ = "artemis/settings/netbird_unavailable";
    return false;
#else
    lastError_.clear();

    if (Settings::instance().remote_access_provider() !=
        RemoteAccessProviderId::NetBird) {
        vpn_log(VpnFileLogger::Severity::Warning,
                "start ignored because another provider is selected");
        return false;
    }

    const std::string server = Settings::instance().netbird_server();
    const std::string setupKey = Settings::instance().netbird_setup_key();
    // Log these: without them a misconfigured start looks identical to a silent
    // failure in vpn.log, since nothing else is written before the login.
    if (server.empty()) {
        lastError_ = "artemis/settings/netbird_missing_server";
        vpn_log(VpnFileLogger::Severity::Error, "no management server configured");
        return false;
    }
    if (setupKey.empty()) {
        lastError_ = "artemis/settings/netbird_missing_setup_key";
        vpn_log(VpnFileLogger::Severity::Error, "no setup key configured");
        return false;
    }
    // Never log the key; its length is enough to tell a loaded key from a
    // truncated or empty one.
    vpn_log(VpnFileLogger::Severity::Info,
            "setup key present (" + std::to_string(setupKey.size()) + " chars)");

    // Never log the setup key itself.
    brls::Logger::info("NetBird: logging in to {}", server);
    vpn_log(VpnFileLogger::Severity::Info, "logging in to " + server);

    // netbird_init() performs the whole documented flow: setup-key login
    // against the management server, peer sync, relay connect, WireGuard key
    // derivation and handshake. Artemis must not reimplement any of that.
    char error[256]{};
    const int rc = netbird_init(server.c_str(), setupKey.c_str(), error,
                                sizeof(error));
    if (rc != 0) {
        lastError_ = error[0] ? error : "artemis/settings/netbird_login_failed";
        brls::Logger::warning("NetBird: login failed ({})", lastError_);
        vpn_log(VpnFileLogger::Severity::Error, "login failed: " + lastError_);
        return false;
    }

    started_ = true;
    brls::Logger::info("NetBird: connected, tunnel address {}", localAddress());
    vpn_log(VpnFileLogger::Severity::Info, "connected, tunnel address " + localAddress());

    // Start the timer/packet pump before the first reachability probe. This
    // also keeps NetBird progressing while gs_init() blocks the UI thread.
    startPump();
    vpn_log(VpnFileLogger::Severity::Info,
            "authenticated peer sync returned " +
                std::to_string(std::max(netbird_get_peer_count(), 0)) +
                " entries");

    // start() is already on a worker thread, so seed the peer cache here rather
    // than making the first peers() caller pay for it.
    refreshPeers();
    return true;
#endif
}

void NetBirdProvider::poll() {
    // NetBird owns a persistent pump thread. Frame-driven polling would leave
    // the tunnel idle while the UI thread is blocked in a GameStream request.
}

void NetBirdProvider::stop() {
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    if (!started_) {
        return;
    }
    vpn_log(VpnFileLogger::Severity::Info, "stopping transport");

    // Tear down in reverse order of setup so no worker thread is left holding
    // a socket that is about to be closed underneath it.
    netbird_proxy_stop_udp();
    netbird_proxy_stop();
    stopPump();
    netbird_shutdown();

    activePeer_.clear();
    udpRelaysStarted_ = false;
    started_ = false;

    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        cachedPeers_.clear();
    }
#endif
}

void NetBirdProvider::startPump() {
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    if (pumpRunning_.exchange(true, std::memory_order_acq_rel)) {
        return;
    }

    pumpPollCount_.store(0, std::memory_order_release);
    pumpMaxGapMs_.store(0, std::memory_order_release);
    pumpLastPollMs_.store(steady_milliseconds(), std::memory_order_release);

    pumpThread_ = std::thread([this] {
        auto previous = std::chrono::steady_clock::now();
        auto nextWatchdog = previous + kPumpWatchdogInterval;

        while (pumpRunning_.load(std::memory_order_acquire)) {
            const auto now = std::chrono::steady_clock::now();
            const auto gap = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - previous)
                    .count());
            previous = now;

            auto maximum = pumpMaxGapMs_.load(std::memory_order_relaxed);
            while (gap > maximum &&
                   !pumpMaxGapMs_.compare_exchange_weak(
                       maximum, gap, std::memory_order_relaxed)) {
            }

            netbird_poll();
            pumpPollCount_.fetch_add(1, std::memory_order_relaxed);
            pumpLastPollMs_.store(steady_milliseconds(),
                                  std::memory_order_release);

            if (now >= nextWatchdog) {
                const auto maxGap =
                    pumpMaxGapMs_.exchange(0, std::memory_order_acq_rel);
                vpn_log(maxGap > 100 ? VpnFileLogger::Severity::Warning
                                     : VpnFileLogger::Severity::Info,
                        "background pump alive: polls=" +
                            std::to_string(pumpPollCount_.load(
                                std::memory_order_relaxed)) +
                            " max_gap=" + std::to_string(maxGap) +
                            "ms ready=" +
                            (netbird_is_ready() ? "yes" : "no") +
                            " peers=" +
                            std::to_string(std::max(
                                netbird_get_peer_count(), 0)));
                nextWatchdog = now + kPumpWatchdogInterval;
            }

            svcSleepThread(
                static_cast<std::int64_t>(kPumpInterval.count()) * 1000000);
        }
    });
    vpn_log(VpnFileLogger::Severity::Info,
            "NetBird background pump started (interval 5 ms)");
#endif
}

void NetBirdProvider::stopPump() {
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    if (!pumpRunning_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (pumpThread_.joinable()) {
        pumpThread_.join();
    }
    vpn_log(VpnFileLogger::Severity::Info,
            "NetBird background pump stopped");
#endif
}

std::string NetBirdProvider::status() const {
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    if (!lastError_.empty()) {
        return lastError_;
    }
    if (!started_) {
        return "artemis/settings/remote_access_off";
    }
    return netbird_is_ready() ? "artemis/settings/remote_access_connected"
                              : "artemis/settings/remote_access_connecting";
#else
    return "artemis/settings/netbird_unavailable";
#endif
}

std::string NetBirdProvider::lastError() const { return lastError_; }

std::string NetBirdProvider::localAddress() const {
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    if (!started_) {
        return {};
    }
    const char* ip = netbird_get_ip();
    return ip ? ip : std::string{};
#else
    return {};
#endif
}

std::vector<RemoteAccessPeer> NetBirdProvider::peers() const {
    // Cache read only. Probing every peer takes a network round trip each, so
    // it must never happen on whatever thread is asking for the list -- the
    // settings status ticker calls this once a second.
    std::lock_guard<std::mutex> lock(peersMutex_);
    return cachedPeers_;
}

void NetBirdProvider::refreshPeers() {
    std::vector<RemoteAccessPeer> refreshed;
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    if (started_ && netbird_is_ready()) {
        const int count = netbird_get_peer_count();
        refreshed.reserve(static_cast<size_t>(std::max(count, 0)));
        for (int i = 0; i < count; ++i) {
            char ip[64]{};
            char name[256]{};
            if (!netbird_get_peer(i, ip, sizeof(ip), name, sizeof(name))) {
                continue;
            }

            RemoteAccessPeer peer;
            peer.providerId = "netbird";
            peer.peerId = ip;
            peer.name = name[0] ? name : ip;
            // The real mesh address. Callers keep this as host identity and
            // only stream through 127.0.0.1 once a route is active.
            peer.address = ip;
            // Only advertise peers that actually answer on the GameStream
            // port, so a NAS or phone on the mesh is not offered as a host.
            // BLOCKING: this is why refreshPeers() may not run on the UI thread.
            const auto probeStarted = std::chrono::steady_clock::now();
            const auto pollsBefore =
                pumpPollCount_.load(std::memory_order_acquire);
            const auto lastPoll =
                pumpLastPollMs_.load(std::memory_order_acquire);
            const auto pumpAge = steady_milliseconds() >= lastPoll
                                     ? steady_milliseconds() - lastPoll
                                     : 0;
            vpn_log(VpnFileLogger::Severity::Info,
                    "peer probe begin " + peer.address + ":" +
                        std::to_string(kGameStreamPort) +
                        " ready=" + (netbird_is_ready() ? "yes" : "no") +
                        " pump=" +
                        (pumpRunning_.load(std::memory_order_acquire)
                             ? "running"
                             : "stopped") +
                        " pump_age=" + std::to_string(pumpAge) + "ms");

            peer.online = netbird_peer_reachable(
                              ip, kGameStreamPort, kPeerProbeTimeoutMs) == 1;
            const auto elapsed = std::chrono::duration_cast<
                std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                           probeStarted);
            const auto pollsAfter =
                pumpPollCount_.load(std::memory_order_acquire);
            vpn_log(VpnFileLogger::Severity::Info,
                    "peer probe " + peer.address + ":" +
                        std::to_string(kGameStreamPort) + " " +
                        (peer.online ? "reachable" : "unreachable") +
                        " elapsed=" + std::to_string(elapsed.count()) +
                        "ms polls_during_probe=" +
                        std::to_string(pollsAfter - pollsBefore) +
                        " max_pump_gap=" +
                        std::to_string(pumpMaxGapMs_.load(
                            std::memory_order_acquire)) +
                        "ms");
            refreshed.push_back(std::move(peer));
        }

        const auto reachable = std::count_if(
            refreshed.begin(), refreshed.end(),
            [](const RemoteAccessPeer& peer) { return peer.online; });
        vpn_log(VpnFileLogger::Severity::Info,
                "peer scan complete: " + std::to_string(reachable) + "/" +
                    std::to_string(refreshed.size()) +
                    " answer on the GameStream HTTP port");
    } else if (started_) {
        vpn_log(VpnFileLogger::Severity::Warning,
                "peer scan skipped because the transport is not ready");
    }
#endif
    std::lock_guard<std::mutex> lock(peersMutex_);
    cachedPeers_ = std::move(refreshed);
}

bool NetBirdProvider::isKnownPeer(const std::string& address) const {
    if (address.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(peersMutex_);
    return std::any_of(cachedPeers_.begin(), cachedPeers_.end(),
                       [&address](const RemoteAccessPeer& peer) {
                           return peer.address == address;
                       });
}

bool NetBirdProvider::activateRoute(const std::string& peerId) {
#if !defined(__SWITCH__) || !defined(ENABLE_NETBIRD)
    (void)peerId;
    return false;
#else
    if (!started_) {
        vpn_log(VpnFileLogger::Severity::Error,
                "route activation rejected because the transport is stopped");
        return false;
    }
    if (!netbird_is_ready()) {
        lastError_ = "artemis/settings/netbird_proxy_failed";
        vpn_log(VpnFileLogger::Severity::Error,
                "route activation rejected because the transport is not ready");
        return false;
    }
    if (peerId.empty()) {
        vpn_log(VpnFileLogger::Severity::Error,
                "route activation rejected because the peer address is empty");
        return false;
    }

    vpn_log(VpnFileLogger::Severity::Info,
            "activating TCP route for peer " + peerId);

    // Only ever route to an address the authenticated peer sync returned.
    // Without this check any 100.x address could redirect traffic into the
    // tunnel.
    bool known = false;
    const int count = netbird_get_peer_count();
    for (int i = 0; i < count && !known; ++i) {
        char ip[64]{};
        char name[256]{};
        if (netbird_get_peer(i, ip, sizeof(ip), name, sizeof(name))) {
            known = peerId == ip;
        }
    }
    if (!known) {
        brls::Logger::warning("NetBird: refusing route to unknown peer {}", peerId);
        vpn_log(VpnFileLogger::Severity::Warning,
                "refusing route to unknown peer " + peerId + " (" +
                    std::to_string(std::max(count, 0)) +
                    " authenticated peers in sync)");
        return false;
    }

    if (activePeer_ == peerId) {
        return true;
    }

    // One peer at a time: stop the previous route before starting the new one.
    if (!activePeer_.empty()) {
        vpn_log(VpnFileLogger::Severity::Info,
                "switching TCP route from " + activePeer_ + " to " + peerId);
    }
    netbird_proxy_stop_udp();
    netbird_proxy_stop();
    udpRelaysStarted_ = false;

    const int proxyResult =
        netbird_proxy_start(peerId.c_str(), kGameStreamPort);
    if (proxyResult != 0) {
        lastError_ = "artemis/settings/netbird_proxy_failed";
        vpn_log(VpnFileLogger::Severity::Error,
                "TCP proxy start failed for peer " + peerId + " (code " +
                    std::to_string(proxyResult) +
                    "; required listeners: 47989 HTTP, 47984 HTTPS, 48010 RTSP)");
        return false;
    }

    activePeer_ = peerId;
    lastError_.clear();
    vpn_log(VpnFileLogger::Severity::Info,
            "TCP route active for peer " + peerId +
                " (listeners 47989/47984/48010; UDP deferred until stream launch)");
    return true;
#endif
}

bool NetBirdProvider::prepareRouteForStreaming(const std::string& peerId) {
#if !defined(__SWITCH__) || !defined(ENABLE_NETBIRD)
    (void)peerId;
    return false;
#else
    if (!started_ || !netbird_is_ready() || peerId.empty() ||
        activePeer_ != peerId) {
        lastError_ = "artemis/settings/netbird_proxy_failed";
        vpn_log(VpnFileLogger::Severity::Error,
                "UDP relay start rejected for peer " + peerId +
                    " because its TCP route is not active");
        return false;
    }
    if (udpRelaysStarted_) {
        // Relay threads intentionally exit after an idle stream. Stop/join and
        // recreate them for every launch so a second session cannot inherit
        // sockets whose worker threads have already ended.
        vpn_log(VpnFileLogger::Severity::Info,
                "restarting UDP media relays for a new stream launch");
        netbird_proxy_stop_udp();
        udpRelaysStarted_ = false;
    }

    const int udpResult = netbird_proxy_start_udp(peerId.c_str());
    if (udpResult != 0) {
        lastError_ = "artemis/settings/netbird_proxy_failed";
        vpn_log(VpnFileLogger::Severity::Error,
                "UDP relay start failed for peer " + peerId + " (code " +
                    std::to_string(udpResult) +
                    "; required ports: 47998, 48000, 47999, 48002, 48010)");
        return false;
    }

    udpRelaysStarted_ = true;
    lastError_.clear();
    vpn_log(VpnFileLogger::Severity::Info,
            "UDP media relays active for peer " + peerId +
                " (ports 47998/48000/47999/48002/48010)");
    return true;
#endif
}

void NetBirdProvider::deactivateRoute(const std::string& peerId) {
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    if (!started_ || activePeer_.empty() || activePeer_ != peerId) {
        return;
    }
    netbird_proxy_stop_udp();
    netbird_proxy_stop();
    activePeer_.clear();
    udpRelaysStarted_ = false;
    vpn_log(VpnFileLogger::Severity::Info,
            "route released for peer " + peerId);
#else
    (void)peerId;
#endif
}
