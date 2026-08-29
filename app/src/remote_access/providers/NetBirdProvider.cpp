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

// Artemis extension applied only to the staged NetBird build. Keep the pinned
// submodule header pristine while exposing the staged ABI to this adapter.
bool netbird_get_peer_identity(int index, char* identity_out,
                               size_t identity_size);
}
#endif

using namespace brls::literals;

namespace {

// Moonlight's GameStream HTTP port. The TCP proxy is anchored here; the UDP
// relay covers the video/audio/control ports on its own.
constexpr uint16_t kGameStreamPort = 47989;

// Backstop for a control plane that reports an absurd peer count. Bound the
// scan and its allocations so a single bogus reply cannot exhaust the heap.
constexpr int kMaxScannedPeers = 256;
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
    if (stateSnapshot().started)
        stop();
    else
        stopPump();
}

std::string NetBirdProvider::id() const { return "netbird"; }
std::string NetBirdProvider::name() const { return "NetBird"; }

NetBirdProvider::StateSnapshot NetBirdProvider::stateSnapshot() const {
    std::lock_guard lock(stateMutex_);
    return state_;
}

void NetBirdProvider::setLastError(std::string error) {
    std::lock_guard lock(stateMutex_);
    state_.lastError = std::move(error);
}

bool NetBirdProvider::available() const {
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    return true;
#else
    return false;
#endif
}

bool NetBirdProvider::start() {
#if !defined(__SWITCH__) || !defined(ENABLE_NETBIRD)
    setLastError("artemis/settings/netbird_unavailable");
    return false;
#else
    setLastError({});

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
        setLastError("artemis/settings/netbird_missing_server");
        vpn_log(VpnFileLogger::Severity::Error, "no management server configured");
        return false;
    }
    if (setupKey.empty()) {
        setLastError("artemis/settings/netbird_missing_setup_key");
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
        const std::string failure =
            error[0] ? error : "artemis/settings/netbird_login_failed";
        setLastError(failure);
        brls::Logger::warning("NetBird: login failed ({})", failure);
        vpn_log(VpnFileLogger::Severity::Error, "login failed: " + failure);
        return false;
    }

    const char* reportedIp = netbird_get_ip();
    const std::string tunnelAddress = reportedIp ? reportedIp : std::string{};
    {
        std::lock_guard lock(stateMutex_);
        state_.started = true;
        state_.ready = netbird_is_ready();
        state_.localAddress = tunnelAddress;
        state_.lastError.clear();
    }
    brls::Logger::info("NetBird: connected, tunnel address {}", tunnelAddress);
    vpn_log(VpnFileLogger::Severity::Info,
            "connected, tunnel address " + tunnelAddress);

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
    // The persistent pump keeps timers and packets moving even while the UI
    // thread is blocked in GameStream HTTP.
}

void NetBirdProvider::stop() {
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    if (!stateSnapshot().started) {
        return;
    }
    vpn_log(VpnFileLogger::Severity::Info, "stopping transport");

    // Tear down in reverse order of setup so no worker thread is left holding
    // a socket that is about to be closed underneath it.
    netbird_proxy_stop_udp();
    netbird_proxy_stop();
    stopPump();
    netbird_shutdown();

    {
        std::lock_guard lock(stateMutex_);
        state_ = StateSnapshot{};
    }

    {
        std::lock_guard<std::mutex> lock(peersMutex_);
        cachedPeers_.clear();
    }
#endif
}

void NetBirdProvider::startPump() {
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    if (pumpRunning_.exchange(true, std::memory_order_acq_rel))
        return;

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
            const bool ready = netbird_is_ready();
            {
                std::lock_guard lock(stateMutex_);
                if (state_.started)
                    state_.ready = ready;
            }
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
                            (ready ? "yes" : "no") +
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
    if (!pumpRunning_.exchange(false, std::memory_order_acq_rel))
        return;
    if (pumpThread_.joinable())
        pumpThread_.join();
    vpn_log(VpnFileLogger::Severity::Info,
            "NetBird background pump stopped");
#endif
}

std::string NetBirdProvider::status() const {
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    const auto snapshot = stateSnapshot();
    if (!snapshot.lastError.empty()) {
        return snapshot.lastError;
    }
    if (!snapshot.started) {
        return "artemis/settings/remote_access_off";
    }
    return snapshot.ready ? "artemis/settings/remote_access_connected"
                          : "artemis/settings/remote_access_connecting";
#else
    return "artemis/settings/netbird_unavailable";
#endif
}

std::string NetBirdProvider::lastError() const {
    return stateSnapshot().lastError;
}

std::string NetBirdProvider::localAddress() const {
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    return stateSnapshot().localAddress;
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
    const auto state = stateSnapshot();
    if (state.started && state.ready) {
        const int reported = netbird_get_peer_count();
        const int count = std::min(reported, kMaxScannedPeers);
        if (reported != count) {
            vpn_log(VpnFileLogger::Severity::Warning,
                    "control plane reported " + std::to_string(reported) +
                        " peers; scanning first " + std::to_string(count));
        }
        refreshed.reserve(static_cast<size_t>(std::max(count, 0)));
        for (int i = 0; i < count; ++i) {
            char ip[64]{};
            char name[256]{};
            char identity[96]{};
            if (!netbird_get_peer(i, ip, sizeof(ip), name, sizeof(name))) {
                continue;
            }
            if (!netbird_get_peer_identity(i, identity, sizeof(identity))) {
                vpn_log(VpnFileLogger::Severity::Warning,
                        "peer skipped because its authenticated identity is invalid");
                continue;
            }

            RemoteAccessPeer peer;
            peer.providerId = "netbird";
            peer.peerId = identity;
            peer.name = name[0] ? name : ip;
            // The real mesh address. Callers keep this as host identity and
            // only stream through 127.0.0.1 once a route is active.
            peer.address = ip;
            // Presence in this list already came from the authenticated
            // management sync. A direct 100.x:47989 probe is invalid before
            // Artemis activates the peer's loopback relay, and caused working
            // hosts to be hidden as "0 / 1 reachable". Offer authenticated
            // peers to auto-search; route activation and the GameStream HTTP
            // handshake remain the authoritative reachability check.
            peer.online = true;
            peer.metadata = "Authenticated NetBird peer";
            refreshed.push_back(std::move(peer));
        }
        vpn_log(VpnFileLogger::Severity::Info,
                "peer sync complete: " + std::to_string(refreshed.size()) +
                    " authenticated peers eligible for auto-search; "
                    "GameStream is checked after route activation");
    } else if (state.started) {
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

std::optional<RemoteRouteTarget>
NetBirdProvider::resolveRoute(std::string_view address) const {
    if (address.empty())
        return std::nullopt;
    const std::string candidate(address);
    std::lock_guard<std::mutex> lock(peersMutex_);
    const auto peer = std::find_if(
        cachedPeers_.begin(), cachedPeers_.end(),
        [&candidate](const RemoteAccessPeer& item) {
            return item.address == candidate;
        });
    if (peer == cachedPeers_.end())
        return std::nullopt;
    return RemoteRouteTarget{
        peer->peerId.empty() ? std::string("netbird:") + peer->address
                             : peer->peerId,
        peer->address, candidate, "127.0.0.1", RemoteRouteMode::Proxy};
}

bool NetBirdProvider::activateRoute(const RemoteRouteTarget& target) {
    const std::string& peerId = target.peerAddress;
#if !defined(__SWITCH__) || !defined(ENABLE_NETBIRD)
    (void)peerId;
    return false;
#else
    auto state = stateSnapshot();
    if (!state.started) {
        vpn_log(VpnFileLogger::Severity::Error,
                "route activation rejected because the transport is stopped");
        return false;
    }
    if (!state.ready) {
        setLastError("artemis/settings/netbird_proxy_failed");
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

    // Only ever route to an address the authenticated peer sync returned,
    // AND only when the stable identity the caller asked for is the one that
    // actually holds that mesh address. Without the identity binding a recycled
    // 100.x address could silently redirect one peer's route to another.
    bool known = false;
    const auto reported = netbird_get_peer_count();
    const int count = std::min(std::max(reported, 0), kMaxScannedPeers);
    for (int i = 0; i < count && !known; ++i) {
        char ip[64]{};
        char name[256]{};
        char identity[96]{};
        if (!netbird_get_peer(i, ip, sizeof(ip), name, sizeof(name)))
            continue;
        if (peerId != ip)
            continue;
        if (!netbird_get_peer_identity(i, identity, sizeof(identity))) {
            setLastError("artemis/settings/netbird_route_refused");
            vpn_log(VpnFileLogger::Severity::Warning,
                    "refusing route: peer " + peerId +
                        " has no valid authenticated identity");
            return false;
        }
        if (!target.peerId.empty() && target.peerId != identity) {
            setLastError("artemis/settings/netbird_route_refused");
            vpn_log(VpnFileLogger::Severity::Warning,
                    "refusing route: mesh address " + peerId +
                        " belongs to identity " + std::string(identity) +
                        ", not requested " + target.peerId);
            return false;
        }
        known = true;
    }
    if (!known) {
        brls::Logger::warning("NetBird: refusing route to unknown peer {}", peerId);
        vpn_log(VpnFileLogger::Severity::Warning,
                "refusing route to unknown peer " + peerId + " (" +
                    std::to_string(count) +
                    " authenticated peers in sync)");
        return false;
    }

    state = stateSnapshot();
    if (state.activePeer == peerId) {
        return true;
    }

    if (!state.activePeer.empty()) {
        vpn_log(VpnFileLogger::Severity::Info,
                "switching TCP route from " + state.activePeer + " to " +
                    peerId);
    }
    netbird_proxy_stop_udp();
    netbird_proxy_stop();
    {
        std::lock_guard lock(stateMutex_);
        state_.udpRelaysStarted = false;
    }

    const int proxyResult =
        netbird_proxy_start(peerId.c_str(), kGameStreamPort);
    if (proxyResult != 0) {
        setLastError("artemis/settings/netbird_proxy_failed");
        vpn_log(VpnFileLogger::Severity::Error,
                "TCP proxy start failed for peer " + peerId + " (code " +
                    std::to_string(proxyResult) +
                    "; required listeners: 47989 HTTP, 47984 HTTPS, 48010 RTSP)");
        return false;
    }

    {
        std::lock_guard lock(stateMutex_);
        state_.activePeer = peerId;
        state_.lastError.clear();
    }
    vpn_log(VpnFileLogger::Severity::Info,
            "TCP route active for peer " + peerId +
                " (listeners 47989/47984/48010; UDP deferred until stream launch)");
    return true;
#endif
}

bool NetBirdProvider::prepareRouteForStreaming(
    const RemoteRouteTarget& target) {
    const std::string& peerId = target.peerAddress;
#if !defined(__SWITCH__) || !defined(ENABLE_NETBIRD)
    (void)peerId;
    return false;
#else
    auto state = stateSnapshot();
    if (!state.started || !state.ready || peerId.empty() ||
        state.activePeer != peerId) {
        setLastError("artemis/settings/netbird_proxy_failed");
        vpn_log(VpnFileLogger::Severity::Error,
                "UDP relay start rejected for peer " + peerId +
                    " because its TCP route is not active");
        return false;
    }
    if (state.udpRelaysStarted) {
        vpn_log(VpnFileLogger::Severity::Info,
                "restarting UDP media relays for a new stream launch");
        netbird_proxy_stop_udp();
        {
            std::lock_guard lock(stateMutex_);
            state_.udpRelaysStarted = false;
        }
    }

    const int udpResult = netbird_proxy_start_udp(peerId.c_str());
    if (udpResult != 0) {
        setLastError("artemis/settings/netbird_proxy_failed");
        vpn_log(VpnFileLogger::Severity::Error,
                "UDP relay start failed for peer " + peerId + " (code " +
                    std::to_string(udpResult) +
                    "; required ports: 47998, 48000, 47999, 48002, 48010)");
        return false;
    }

    {
        std::lock_guard lock(stateMutex_);
        state_.udpRelaysStarted = true;
        state_.lastError.clear();
    }
    vpn_log(VpnFileLogger::Severity::Info,
            "UDP media relays active for peer " + peerId +
                " (ports 47998/48000/47999/48002/48010)");
    return true;
#endif
}

void NetBirdProvider::deactivateRoute(const RemoteRouteTarget& target) {
    const std::string& peerId = target.peerAddress;
#if defined(__SWITCH__) && defined(ENABLE_NETBIRD)
    const auto state = stateSnapshot();
    if (!state.started || state.activePeer.empty() ||
        state.activePeer != peerId) {
        return;
    }
    netbird_proxy_stop_udp();
    netbird_proxy_stop();
    {
        std::lock_guard lock(stateMutex_);
        state_.activePeer.clear();
        state_.udpRelaysStarted = false;
    }
    vpn_log(VpnFileLogger::Severity::Info,
            "route released for peer " + peerId);
#else
    (void)peerId;
#endif
}
