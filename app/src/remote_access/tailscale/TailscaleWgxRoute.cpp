#include "TailscaleWgxRoute.hpp"
#include "TailscalePeerDirectory.hpp"

#if defined(__SWITCH__) && defined(ENABLE_TAILSCALE)
extern "C" {
#include "wgx.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
}
#include <netdb.h>
#include <poll.h>
#include <unistd.h>
#include "TailscaleDerp.hpp"
#include "TailscaleDisco.hpp"
#include "TailscaleRandom.hpp"
#include "TailscaleTransport.hpp"
#include "../../vpn/SocketFdLock.hpp"
#include "../../utils/Settings.hpp"
#include "../../vpn/VpnFileLogger.hpp"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <functional>
#include <iterator>
#include <optional>
#include <span>
#include <thread>
#include <vector>

// Same shim pattern as TailscaleControlSession: the Switch build links
// X25519 through wg-nx rather than monocypher directly.
extern "C" {
void tailscale_internal_crypto_x25519_public_key(uint8_t[32], const uint8_t[32]);
}
#endif

#include <algorithm>
#include <charconv>
#include <cstring>

namespace artemis::tailscale {

bool SimulatedWgxBackend::startTunnel(const Key32& privateKey,
                                      const std::string& localIp,
                                      std::string* error) {
    const bool allZero =
        std::all_of(privateKey.begin(), privateKey.end(),
                    [](std::uint8_t byte) { return byte == 0; });
    if (allZero) {
        if (error)
            *error = "Simulated backend: private key is uninitialized";
        return false;
    }
    if (localIp.empty() || !PeerDirectory::isLiteralIPv4(localIp)) {
        if (error)
            *error = "Simulated backend: invalid local IP address: " + localIp;
        return false;
    }
    privateKey_ = privateKey;
    localIp_ = localIp;
    running_ = true;
    return true;
}

bool SimulatedWgxBackend::addOrUpdatePeer(uint32_t peerId,
                                          const Key32& publicKey,
                                          const std::string& peerIp,
                                          std::string* error) {
    if (!running_) {
        if (error)
            *error = "Simulated backend: tunnel not running";
        return false;
    }
    if (peerId == 0) {
        if (error)
            *error = "Simulated backend: peer ID must be non-zero";
        return false;
    }
    const bool allZero =
        std::all_of(publicKey.begin(), publicKey.end(),
                    [](std::uint8_t byte) { return byte == 0; });
    if (allZero) {
        if (error)
            *error = "Simulated backend: public key is uninitialized";
        return false;
    }
    if (peerIp.empty() || !PeerDirectory::isLiteralIPv4(peerIp)) {
        if (error)
            *error = "Simulated backend: invalid peer IP address: " + peerIp;
        return false;
    }
    activePeerIp_ = peerIp;
    return true;
}

bool SimulatedWgxBackend::startTcpProxy(const std::string& peerIp,
                                        const std::string& hostIp,
                                        std::span<const std::uint16_t> ports,
                                        std::string* error) {
    if (!running_) {
        if (error)
            *error = "Simulated backend: tunnel not running";
        return false;
    }
    if (ports.empty()) {
        if (error)
            *error = "Simulated backend: no TCP ports specified";
        return false;
    }
    activePeerIp_ = peerIp;
    activeHostIp_ = hostIp;
    tcpPorts_.assign(ports.begin(), ports.end());
    tcpActive_ = true;
    return true;
}

bool SimulatedWgxBackend::startUdpRelay(const std::string& peerIp,
                                        std::span<const std::uint16_t> ports,
                                        std::string* error) {
    if (!running_ || !tcpActive_) {
        if (error)
            *error =
                "Simulated backend: cannot start UDP without active TCP route";
        return false;
    }
    if (ports.empty()) {
        if (error)
            *error = "Simulated backend: no UDP ports specified";
        return false;
    }
    activePeerIp_ = peerIp;
    udpPorts_.assign(ports.begin(), ports.end());
    udpActive_ = true;
    return true;
}

void SimulatedWgxBackend::stopUdpRelay() noexcept {
    udpActive_ = false;
    udpPorts_.clear();
}

bool SimulatedWgxBackend::ensureDerpRoute(const Key32& peerNodeKey,
                                          int homeDerpRegion,
                                          const std::vector<DerpRegion>& derpMap,
                                          const Key32& localPrivateKey,
                                          std::string* error) {
    (void)derpMap;
    (void)localPrivateKey;
    if (!running_) {
        if (error)
            *error = "Simulated backend: tunnel not running";
        return false;
    }
    const bool keyZero =
        std::all_of(peerNodeKey.begin(), peerNodeKey.end(),
                    [](std::uint8_t byte) { return byte == 0; });
    if (keyZero) {
        if (error)
            *error = "Simulated backend: peer node key is uninitialized";
        return false;
    }
    if (homeDerpRegion <= 0) {
        if (error)
            *error = "Simulated backend: peer has no home DERP region";
        return false;
    }
    derpReady_ = true;
    return true;
}

void SimulatedWgxBackend::stop() noexcept {
    stopUdpRelay();
    tcpActive_ = false;
    tcpPorts_.clear();
    running_ = false;
    derpReady_ = false;
    activePeerIp_.clear();
    activeHostIp_.clear();
    directConfig_.reset();
}

bool SimulatedWgxBackend::isRunning() const noexcept { return running_; }
bool SimulatedWgxBackend::isTcpActive() const noexcept { return tcpActive_; }
bool SimulatedWgxBackend::isUdpActive() const noexcept { return udpActive_; }
bool SimulatedWgxBackend::isDerpReady() const noexcept { return derpReady_; }


#if defined(__SWITCH__) && defined(ENABLE_TAILSCALE)

namespace {
void realIngressCallback(void*, const void*, size_t) {}

void logTsRoute(VpnFileLogger::Severity severity, std::string_view message);

// lwIP relay log sink (levels: 0 debug, 1 info, 2 error). Debug is dropped;
// connection-level info/errors go to vpn.log so TCP connect attempts through
// the tunnel are visible.
void tailscaleRelayLog(int level, const char* message) {
    if (level == 0 || !message)
        return;
    logTsRoute(level >= 2 ? VpnFileLogger::Severity::Error
                          : VpnFileLogger::Severity::Info,
               std::string("relay: ") + message);
}

void logTsRoute(VpnFileLogger::Severity severity, std::string_view message) {
    VpnFileLogger::append(Settings::instance().working_dir() + "/vpn.log",
                          "TS", severity, message);
}

// Bounded byte-wise HTTP response header reader for the DERP upgrade.
// Mirrors the control session's reader; the header ends at the first CRLFCRLF.
bool readDerpUpgradeHeader(ITransport& transport, std::string* header,
                           std::string* error) {
    constexpr std::size_t kMaxHeader = 16 * 1024;
    header->clear();
    std::array<std::uint8_t, 1> byte{};
    while (header->size() < kMaxHeader) {
        const int received = transport.read(byte.data(), 1, error);
        if (received < 0)
            return false;
        if (received == 0) {
            if (error)
                *error = "DERP connection closed during HTTP upgrade";
            return false;
        }
        header->push_back(static_cast<char>(byte[0]));
        if (header->ends_with("\r\n\r\n"))
            return true;
    }
    if (error)
        *error = "oversized DERP upgrade response";
    return false;
}
} // namespace

class RealWgxBackend final : public IWgxBackend {
public:
    ~RealWgxBackend() override { stop(); }

    static void realEgressCallback(void* user, uint32_t peer_id, const void* packet, size_t length) {
        auto* backend = static_cast<RealWgxBackend*>(user);
        // Single active peer: the relay destination is the node key captured
        // at ensureDerpRoute time, not the wgx-internal numeric peer id.
        (void)peer_id;
        if (backend && packet && length > 0)
            backend->sendEgress(packet, length);
    }

    // WireGuard encrypted egress -> DERP SendPacket addressed by peer node key.
    // Runs on the WireGuard worker thread: never blocks on anything except a
    // short TLS write under its own mutex.
    void sendEgress(const void* packet, size_t length) {
        if (length > kDerpMaxPacketSize)
            return;
        // WireGuard message header: one LE u32 type (1 initiation, 2
        // response, 3 cookie, 4 transport). Logged once per type so a
        // malformed stack shows up in vpn.log instead of silent drops.
        if (length >= 4) {
            const auto* bytes = static_cast<const std::uint8_t*>(packet);
            const std::uint32_t msgType =
                static_cast<std::uint32_t>(bytes[0]) |
                (static_cast<std::uint32_t>(bytes[1]) << 8U) |
                (static_cast<std::uint32_t>(bytes[2]) << 16U) |
                (static_cast<std::uint32_t>(bytes[3]) << 24U);
            const unsigned slot = msgType < 8 ? msgType : 0;
            egressCount_[slot].fetch_add(1, std::memory_order_relaxed);
            egressBytes_.fetch_add(length, std::memory_order_relaxed);
            if (!egressTypes_[slot]) {
                egressTypes_[slot] = true;
                logTsRoute(VpnFileLogger::Severity::Info,
                           "DERP relay sending WireGuard type=" +
                               std::to_string(msgType) + " (" +
                               wgTypeName(msgType) + ") len=" +
                               std::to_string(length) + " t+" +
                               std::to_string(msSinceHandshakeStart()) + "ms");
            }
            // First initiation in full: type/mac layout is public data
            // (ephemeral key, opaque ciphertext, MACs). Lets the mac1 be
            // verified offline against the responder's public key.
            if (msgType == 1 && length == 148 && !initiationLogged_) {
                initiationLogged_ = true;
                constexpr char kHex[] = "0123456789abcdef";
                std::string dump;
                dump.reserve(length * 2);
                for (std::size_t i = 0; i < length; ++i) {
                    dump.push_back(kHex[bytes[i] >> 4U]);
                    dump.push_back(kHex[bytes[i] & 0x0fU]);
                }
                logTsRoute(VpnFileLogger::Severity::Info,
                           "WireGuard initiation bytes: " + dump);
            }
        }
        // A pong-proven direct UDP path wins; anything else (no path yet,
        // trust expired, send error) falls through to DERP.
        if (directActive_.load(std::memory_order_acquire) &&
            sendDirect(packet, length))
            return;
        std::lock_guard lock(derpWriteMutex_);
        if (!derpAlive_ || !derp_ || !havePeer_) {
            if (egressDropped_.fetch_add(1, std::memory_order_relaxed) == 0)
                logTsRoute(VpnFileLogger::Severity::Warning,
                           "WireGuard egress dropped: DERP relay is not "
                           "connected (alive=" +
                               std::string(derpAlive_ ? "yes" : "no") +
                               " peer=" + (havePeer_ ? "yes" : "no") + ")");
            return;
        }
        const auto* bytes = static_cast<const std::uint8_t*>(packet);
        std::string sendError;
        if (derp_->sendPacket(std::span<const std::uint8_t>(peerNodeKey_.data(), peerNodeKey_.size()),
                              std::span<const std::uint8_t>(bytes, length), &sendError)) {
            if (!egressLogged_) {
                egressLogged_ = true;
                logTsRoute(VpnFileLogger::Severity::Info,
                           "DERP relay sending peer packets");
            }
        } else if (egressSendFailures_.fetch_add(1, std::memory_order_relaxed) == 0) {
            logTsRoute(VpnFileLogger::Severity::Error,
                       "DERP SendPacket write failed: " +
                           (sendError.empty() ? std::string("unknown error")
                                              : sendError));
        }
    }

    // ---- Direct UDP path (Settings: tailscale_direct_connections) ----
    //
    // One UDP socket carries STUN, disco and direct WireGuard. WireGuard
    // stays on DERP until a disco ping to one of the peer's endpoints is
    // answered; the path is then trusted for DirectPath::kTrustDuration and
    // re-proven on every heartbeat, so a dead path drops back to DERP within
    // a few seconds on its own.

    using DirectClock = DirectPath::Clock;

    static sockaddr_in toSockaddr(const IPv4Endpoint& endpoint) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(endpoint.address);
        addr.sin_port = htons(endpoint.port);
        return addr;
    }

    static IPv4Endpoint fromSockaddr(const sockaddr_in& addr) {
        return {ntohl(addr.sin_addr.s_addr), ntohs(addr.sin_port)};
    }

    // The interface address the OS routes to the internet with. A connected
    // UDP socket sends nothing; getsockname just reports the chosen source.
    static std::optional<IPv4Endpoint> discoverLanAddress() {
        int fd = -1;
        {
            auto guard = SocketFdLock::instance().guard();
            fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        }
        if (fd < 0)
            return std::nullopt;
        sockaddr_in probe{};
        probe.sin_family = AF_INET;
        probe.sin_port = htons(53);
        probe.sin_addr.s_addr = htonl(0x08080808U);
        std::optional<IPv4Endpoint> result;
        if (::connect(fd, reinterpret_cast<sockaddr*>(&probe), sizeof(probe)) == 0) {
            sockaddr_in local{};
            socklen_t len = sizeof(local);
            if (::getsockname(fd, reinterpret_cast<sockaddr*>(&local), &len) == 0 &&
                local.sin_addr.s_addr != 0)
                result = IPv4Endpoint{ntohl(local.sin_addr.s_addr), 1};
        }
        auto guard = SocketFdLock::instance().guard();
        ::close(fd);
        return result;
    }

    void startDirectPath(DirectPathConfig config) override {
        stopDirectPath();
        if (!Settings::instance().tailscale_direct_connections()) {
            logTsRoute(VpnFileLogger::Severity::Info,
                       "direct connections are off in Settings; all traffic "
                       "stays on DERP");
            return;
        }
        int fd = -1;
        {
            auto guard = SocketFdLock::instance().guard();
            fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        }
        if (fd < 0) {
            logTsRoute(VpnFileLogger::Severity::Warning,
                       "direct path: no UDP socket available (errno " +
                           std::to_string(errno) + "); staying on DERP");
            return;
        }
        // Tailscale's default port first (friendlier to port-forwarding
        // routers), any free port otherwise.
        sockaddr_in bindAddr{};
        bindAddr.sin_family = AF_INET;
        bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        bindAddr.sin_port = htons(41641);
        if (::bind(fd, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) != 0) {
            bindAddr.sin_port = 0;
            if (::bind(fd, reinterpret_cast<sockaddr*>(&bindAddr),
                       sizeof(bindAddr)) != 0) {
                logTsRoute(VpnFileLogger::Severity::Warning,
                           "direct path: UDP bind failed (errno " +
                               std::to_string(errno) + "); staying on DERP");
                auto guard = SocketFdLock::instance().guard();
                ::close(fd);
                return;
            }
        }
        sockaddr_in bound{};
        socklen_t boundLen = sizeof(bound);
        ::getsockname(fd, reinterpret_cast<sockaddr*>(&bound), &boundLen);
        const std::uint16_t localPort = ntohs(bound.sin_port);

        Key32 discoPublic{};
        tailscale_internal_crypto_x25519_public_key(
            discoPublic.data(), config.localDiscoPrivate.data());
        Key32 nodePublic{};
        tailscale_internal_crypto_x25519_public_key(
            nodePublic.data(), config.localNodePrivate.data());
        const auto lan = discoverLanAddress();

        std::lock_guard lock(directMutex_);
        directPath_.reset();
        std::string candidateList;
        for (const auto& text : config.peerEndpoints) {
            if (const auto endpoint = IPv4Endpoint::parse(text)) {
                directPath_.addCandidate(*endpoint);
                if (!candidateList.empty())
                    candidateList += ",";
                candidateList += endpoint->toString();
            }
        }
        const std::size_t candidates = directPath_.candidateCount();
        for (auto* counter : {&pingsSent_, &udpSendFailures_, &stunSent_,
                              &udpPacketsReceived_, &stunReceived_,
                              &udpDiscoReceived_, &udpOtherReceived_,
                              &udpRecvFailures_, &pingsFromPeerUdp_,
                              &pingsFromPeerDerp_, &pongsFromPeer_,
                              &pongsMatched_, &callMeMaybeFromPeer_})
            counter->store(0, std::memory_order_relaxed);
        unmatchedPongLogged_ = false;
        stunUnparsedLogged_ = false;
        stunRequestLogged_ = false;
        loopbackEchoed_ = false;
        dnsAnswered_ = false;
        progressLogged_ = false;
        directStarted_ = DirectClock::now();
        directConfig_ = std::move(config);
        localDiscoPublic_ = discoPublic;
        localNodePublic_ = nodePublic;
        udpFd_ = fd;
        udpPort_ = localPort;
        reportedBest_ = {};
        stunMapped_ = {};
        localEndpoints_.clear();
        if (lan)
            localEndpoints_.push_back({lan->address, localPort});
        endpointsChanged_ = true;
        directWgLogged_ = false;
        firstPongLogged_ = false;
        directSent_ = 0;
        directReceived_ = 0;
        discoRejected_ = 0;
        directActive_ = false;
        udpRunning_ = true;
        udpThread_ = std::thread(&RealWgxBackend::udpLoop, this);
        logTsRoute(VpnFileLogger::Severity::Info,
                   "direct path: probing on UDP port " +
                       std::to_string(localPort) + " (LAN " +
                       (lan ? IPv4Endpoint{lan->address, localPort}.toString()
                            : std::string("unknown")) +
                       ", " + std::to_string(candidates) +
                       " peer endpoints from the netmap: {" + candidateList +
                       "}); WireGuard stays on DERP until a path answers");
    }

    void stopDirectPath() noexcept {
        udpRunning_ = false;
        directActive_ = false;
        if (udpThread_.joinable())
            udpThread_.join();
        std::function<void(const std::string&, int)> notify;
        {
            std::lock_guard lock(directMutex_);
            if (udpFd_ >= 0) {
                auto guard = SocketFdLock::instance().guard();
                ::close(udpFd_);
                udpFd_ = -1;
            }
            if (directConfig_) {
                logTsRoute(VpnFileLogger::Severity::Info,
                           "direct path stopped: sent " +
                               std::to_string(directSent_.load()) +
                               " / received " +
                               std::to_string(directReceived_.load()) +
                               " WireGuard packets directly, " +
                               std::to_string(discoRejected_.load()) +
                               " disco packets rejected; " +
                               directCounterLine());
                if (reportedBest_.valid())
                    notify = directConfig_->pathChanged;
            }
            directConfig_.reset();
            directPath_.reset();
            reportedBest_ = {};
        }
        if (notify)
            notify({}, -1);
    }

    bool sendDirect(const void* packet, std::size_t length) {
        std::lock_guard lock(directMutex_);
        if (udpFd_ < 0)
            return false;
        const auto best = directPath_.best(DirectClock::now());
        if (!best)
            return false;
        const auto to = toSockaddr(*best);
        const auto sent = ::sendto(udpFd_, packet, length, 0,
                                   reinterpret_cast<const sockaddr*>(&to),
                                   sizeof(to));
        if (sent != static_cast<decltype(sent)>(length))
            return false;
        directSent_.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    // Caller holds directMutex_.
    bool sendUdpLocked(std::span<const std::uint8_t> bytes,
                       const IPv4Endpoint& to) {
        if (udpFd_ < 0 || bytes.empty())
            return false;
        const auto addr = toSockaddr(to);
        const auto sent = ::sendto(udpFd_, bytes.data(), bytes.size(), 0,
                                   reinterpret_cast<const sockaddr*>(&addr),
                                   sizeof(addr));
        if (sent == static_cast<ssize_t>(bytes.size()))
            return true;
        const int err = errno;
        if (udpSendFailures_.fetch_add(1, std::memory_order_relaxed) == 0)
            logTsRoute(VpnFileLogger::Severity::Warning,
                       "direct path: UDP send to " + to.toString() +
                           " failed (rc=" + std::to_string(sent) + " errno " +
                           std::to_string(err) + ")");
        return false;
    }

    std::string directCounterLine() const {
        auto load = [](const std::atomic_uint32_t& v) {
            return std::to_string(v.load(std::memory_order_relaxed));
        };
        return "pings sent=" + load(pingsSent_) +
               " sendFail=" + load(udpSendFailures_) +
               " stun sent=" + load(stunSent_) +
               " | udp received=" + load(udpPacketsReceived_) +
               " (stun=" + load(stunReceived_) +
               " disco=" + load(udpDiscoReceived_) +
               " wg=" + std::to_string(directReceived_.load()) +
               " other=" + load(udpOtherReceived_) +
               ") recvFail=" + load(udpRecvFailures_) +
               " | disco from peer: ping udp/derp=" + load(pingsFromPeerUdp_) +
               "/" + load(pingsFromPeerDerp_) + " pong=" + load(pongsFromPeer_) +
               " (matched " + load(pongsMatched_) + ") callMeMaybe=" +
               load(callMeMaybeFromPeer_) + " rejected=" +
               std::to_string(discoRejected_.load()) +
               " | candidates=" + std::to_string(directPath_.candidateCount()) +
               " | self-test loopback=" +
               (loopbackEchoed_.load() ? "ok" : "none") +
               " dns=" + (dnsAnswered_.load() ? "ok" : "none");
    }

    static constexpr std::array<std::uint8_t, 16> kLoopbackMarker = {
        'a', 'r', 't', 'e', 'm', 'i', 's', '-', 'u', 'd', 'p', '-',
        't', 'e', 's', 't'};
    static constexpr std::uint32_t kDnsProbeServer = 0x08080808U; // 8.8.8.8

    // Two probes on the direct-path socket that do not depend on Tailscale:
    //  * loopback: a packet to our own port must come back through recvfrom,
    //    proving the receive path (poll + recvfrom) works at all;
    //  * DNS: a query to 8.8.8.8:53 must be answered through the NAT, proving
    //    this network passes UDP replies to this socket.
    void sendSelfTests() {
        const auto probe = [this](const IPv4Endpoint& to,
                                  std::span<const std::uint8_t> bytes,
                                  const char* what) {
            const auto addr = toSockaddr(to);
            const auto sent = ::sendto(udpFd_, bytes.data(), bytes.size(), 0,
                                       reinterpret_cast<const sockaddr*>(&addr),
                                       sizeof(addr));
            const int err = errno;
            const bool ok = sent == static_cast<ssize_t>(bytes.size());
            // errno is only meaningful when the send failed.
            logTsRoute(ok ? VpnFileLogger::Severity::Info
                          : VpnFileLogger::Severity::Warning,
                       std::string("direct path self-test: ") +
                           (ok ? "sent " : "could not send ") + what + " to " +
                           to.toString() +
                           (ok ? std::string{}
                               : " (rc=" + std::to_string(sent) + " errno " +
                                     std::to_string(err) + ")"));
        };
        probe({0x7f000001U, udpPort_}, kLoopbackMarker, "loopback probe");
        secureRandomBytes(std::span<std::uint8_t>(dnsId_.data(), dnsId_.size()));
        std::vector<std::uint8_t> query{dnsId_[0], dnsId_[1], 0x01, 0x00,
                                        0x00, 0x01, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x00};
        static constexpr std::uint8_t kName[] = {9, 't', 'a', 'i', 'l', 's', 'c',
                                                 'a', 'l', 'e', 3, 'c', 'o', 'm', 0};
        query.insert(query.end(), std::begin(kName), std::end(kName));
        query.insert(query.end(), {0x00, 0x01, 0x00, 0x01}); // A, IN
        probe({kDnsProbeServer, 53}, query, "DNS probe");
        selfTestSent_ = DirectClock::now();
    }

    // True when the packet was one of our self-test replies.
    bool handleSelfTest(std::span<const std::uint8_t> packet,
                        const IPv4Endpoint& from) {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            DirectClock::now() - selfTestSent_)
                            .count();
        if (from.address == 0x7f000001U && packet.size() == kLoopbackMarker.size() &&
            std::equal(packet.begin(), packet.end(), kLoopbackMarker.begin())) {
            if (!loopbackEchoed_.exchange(true))
                logTsRoute(VpnFileLogger::Severity::Info,
                           "direct path self-test: loopback packet received "
                           "after " + std::to_string(ms) +
                               " ms (the UDP receive path works)");
            return true;
        }
        if (from.address == kDnsProbeServer && from.port == 53 &&
            packet.size() >= 12 && packet[0] == dnsId_[0] &&
            packet[1] == dnsId_[1]) {
            if (!dnsAnswered_.exchange(true))
                logTsRoute(VpnFileLogger::Severity::Info,
                           "direct path self-test: DNS reply from 8.8.8.8 after " +
                               std::to_string(ms) +
                               " ms (this network passes UDP replies)");
            return true;
        }
        return false;
    }

    // Caller holds directMutex_ with directConfig_ set.
    std::vector<std::uint8_t> sealDiscoLocked(const disco::Message& message) {
        std::array<std::uint8_t, disco::kNonceLen> nonce{};
        secureRandomBytes(nonce);
        return disco::seal(message, directConfig_->localDiscoPrivate,
                           localDiscoPublic_, directConfig_->peerDiscoKey, nonce);
    }

    void sendDiscoViaDerp(const std::vector<std::uint8_t>& packet) {
        if (packet.empty())
            return;
        std::lock_guard lock(derpWriteMutex_);
        if (derpAlive_ && derp_ && havePeer_)
            derp_->sendPacket(
                std::span<const std::uint8_t>(peerNodeKey_.data(),
                                              peerNodeKey_.size()),
                packet, nullptr);
    }

    // Disco from the active peer, either over UDP (`from` set) or DERP.
    void handleDisco(std::span<const std::uint8_t> packet,
                     std::optional<IPv4Endpoint> from) {
        std::vector<std::uint8_t> derpReply;
        {
            std::lock_guard lock(directMutex_);
            if (!directConfig_)
                return; // direct connections off: disco is simply dropped
            const auto opened =
                disco::open(packet, directConfig_->localDiscoPrivate);
            if (!opened || opened->sender != directConfig_->peerDiscoKey) {
                if (discoRejected_.fetch_add(1, std::memory_order_relaxed) == 0)
                    logTsRoute(VpnFileLogger::Severity::Warning,
                               std::string("direct path: rejected a disco "
                                           "packet via ") +
                                   (from ? "UDP from " + from->toString()
                                         : std::string("DERP")) +
                                   (opened ? " (sender is not this peer)"
                                           : " (could not decrypt)"));
                return;
            }
            const auto& message = opened->message;
            switch (message.type) {
            case disco::MessageType::Ping: {
                if ((from ? pingsFromPeerUdp_ : pingsFromPeerDerp_)
                        .fetch_add(1, std::memory_order_relaxed) == 0)
                    logTsRoute(VpnFileLogger::Severity::Info,
                               std::string("direct path: peer pinged us via ") +
                                   (from ? "UDP from " + from->toString()
                                         : std::string("DERP")));
                disco::Message pong;
                pong.type = disco::MessageType::Pong;
                pong.txid = message.txid;
                pong.source =
                    from ? *from
                         : IPv4Endpoint{disco::kDerpMagicAddress,
                                        static_cast<std::uint16_t>(
                                            activeDerpRegion_.load())};
                const auto reply = sealDiscoLocked(pong);
                if (from) {
                    // A peer reaching us directly is itself a candidate.
                    directPath_.addCandidate(*from);
                    sendUdpLocked(reply, *from);
                } else {
                    derpReply = reply;
                }
                break;
            }
            case disco::MessageType::Pong: {
                pongsFromPeer_.fetch_add(1, std::memory_order_relaxed);
                if (!from)
                    break; // we only ping directly
                const auto rtt =
                    directPath_.notePong(message.txid, *from, DirectClock::now());
                if (rtt)
                    pongsMatched_.fetch_add(1, std::memory_order_relaxed);
                else if (!unmatchedPongLogged_) {
                    unmatchedPongLogged_ = true;
                    logTsRoute(VpnFileLogger::Severity::Warning,
                               "direct path: pong from " + from->toString() +
                                   " matches no outstanding ping (late, or "
                                   "answered from another address)");
                }
                if (rtt && !firstPongLogged_) {
                    firstPongLogged_ = true;
                    logTsRoute(VpnFileLogger::Severity::Info,
                               "direct path: pong from " + from->toString() +
                                   " in " + std::to_string(*rtt) + " ms");
                }
                if (message.source.valid() && !(message.source == stunMapped_) &&
                    message.source.address != disco::kDerpMagicAddress) {
                    // How the peer saw us: another endpoint worth advertising.
                    const bool known = std::any_of(
                        localEndpoints_.begin(), localEndpoints_.end(),
                        [&](const IPv4Endpoint& e) { return e == message.source; });
                    if (!known && localEndpoints_.size() < 8) {
                        localEndpoints_.push_back(message.source);
                        endpointsChanged_ = true;
                    }
                }
                break;
            }
            case disco::MessageType::CallMeMaybe: {
                std::string list;
                for (const auto& endpoint : message.endpoints) {
                    directPath_.addCandidate(endpoint);
                    if (!list.empty())
                        list += ",";
                    list += endpoint.toString();
                }
                if (callMeMaybeFromPeer_.fetch_add(1, std::memory_order_relaxed) == 0)
                    logTsRoute(VpnFileLogger::Severity::Info,
                               "direct path: peer asked to be pinged at {" +
                                   list + "}");
                break;
            }
            }
        }
        if (!derpReply.empty())
            sendDiscoViaDerp(derpReply);
    }

    std::optional<sockaddr_in> resolveStunServer() {
        std::vector<DerpRegion> derpMap;
        int region = activeDerpRegion_.load();
        {
            std::lock_guard lock(directMutex_);
            if (!directConfig_)
                return std::nullopt;
            derpMap = directConfig_->derpMap;
            if (region <= 0)
                region = directConfig_->derpRegion;
        }
        for (const auto& candidate : derpMap) {
            if (candidate.regionId != region)
                continue;
            for (const auto& node : candidate.nodes) {
                if (node.stunPort == 0)
                    continue;
                addrinfo hints{};
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_DGRAM;
                addrinfo* result = nullptr;
                if (::getaddrinfo(node.host.c_str(), nullptr, &hints, &result) != 0 ||
                    !result)
                    continue;
                sockaddr_in addr =
                    *reinterpret_cast<const sockaddr_in*>(result->ai_addr);
                ::freeaddrinfo(result);
                addr.sin_port = htons(node.stunPort);
                logTsRoute(VpnFileLogger::Severity::Info,
                           "direct path: STUN server " + node.host + " (" +
                               fromSockaddr(addr).toString() + ")");
                return addr;
            }
        }
        logTsRoute(VpnFileLogger::Severity::Warning,
                   "direct path: no STUN server in DERP region " +
                       std::to_string(region) +
                       "; only LAN and peer-observed endpoints are used");
        return std::nullopt;
    }

    // Timers: STUN refresh, heartbeat pings, call-me-maybe, endpoint
    // publishing and best-path change reporting.
    void directHousekeeping(DirectClock::time_point now,
                            const std::optional<sockaddr_in>& stunServer,
                            DirectClock::time_point& nextStun,
                            DirectClock::time_point& nextCallMeMaybe) {
        std::vector<std::uint8_t> callMeMaybe;
        std::function<void(std::vector<std::string>)> publish;
        std::vector<std::string> published;
        std::function<void(const std::string&, int)> pathChanged;
        std::string changedEndpoint;
        int changedRtt = -1;
        bool pathDidChange = false;
        {
            std::lock_guard lock(directMutex_);
            if (!directConfig_)
                return;
            if (stunServer && now >= nextStun) {
                secureRandomBytes(stunTx_);
                const auto request = stun::bindingRequest(stunTx_);
                const auto sent =
                    ::sendto(udpFd_, request.data(), request.size(), 0,
                             reinterpret_cast<const sockaddr*>(&*stunServer),
                             sizeof(*stunServer));
                const int sendErrno = errno;
                if (!stunRequestLogged_) {
                    stunRequestLogged_ = true;
                    constexpr char kHex[] = "0123456789abcdef";
                    std::string dump;
                    for (const auto byte : request) {
                        dump.push_back(kHex[byte >> 4U]);
                        dump.push_back(kHex[byte & 0x0fU]);
                    }
                    logTsRoute(VpnFileLogger::Severity::Info,
                               "direct path: STUN request (" +
                                   std::to_string(request.size()) +
                                   " bytes) " + dump);
                }
                if (sent == static_cast<ssize_t>(request.size()))
                    stunSent_.fetch_add(1, std::memory_order_relaxed);
                else if (udpSendFailures_.fetch_add(
                             1, std::memory_order_relaxed) == 0)
                    logTsRoute(VpnFileLogger::Severity::Warning,
                               "direct path: STUN send failed (rc=" +
                                   std::to_string(sent) + " errno " +
                                   std::to_string(sendErrno) + ")");
                // Retry quickly until the first answer, then refresh slowly.
                nextStun = now + (stunMapped_.valid() ? std::chrono::seconds(20)
                                                      : std::chrono::seconds(3));
            }
            if (!progressLogged_ && now - directStarted_ >= std::chrono::seconds(15)) {
                progressLogged_ = true;
                if (!directPath_.best(now))
                    logTsRoute(VpnFileLogger::Severity::Warning,
                               "direct path: no direct answer after 15 s; " +
                                   directCounterLine());
            }
            for (const auto& endpoint : directPath_.due(now)) {
                disco::Message ping;
                ping.type = disco::MessageType::Ping;
                secureRandomBytes(ping.txid);
                ping.nodeKey = localNodePublic_;
                ping.hasNodeKey = true;
                if (sendUdpLocked(sealDiscoLocked(ping), endpoint)) {
                    directPath_.notePingSent(ping.txid, endpoint, now);
                    pingsSent_.fetch_add(1, std::memory_order_relaxed);
                } else {
                    // Do not retry every 100 ms: wait for the next heartbeat.
                    directPath_.notePingSent(ping.txid, endpoint, now);
                }
            }
            const auto best = directPath_.best(now);
            // Ask the peer to ping us while there is no direct path, and
            // whenever our endpoints changed.
            if (!localEndpoints_.empty() &&
                (endpointsChanged_ || (!best && now >= nextCallMeMaybe))) {
                disco::Message message;
                message.type = disco::MessageType::CallMeMaybe;
                message.endpoints = localEndpoints_;
                callMeMaybe = sealDiscoLocked(message);
                nextCallMeMaybe = now + std::chrono::seconds(10);
            }
            if (endpointsChanged_) {
                endpointsChanged_ = false;
                publish = directConfig_->publishEndpoints;
                for (const auto& endpoint : localEndpoints_)
                    published.push_back(endpoint.toString());
            }
            const IPv4Endpoint current = best.value_or(IPv4Endpoint{});
            if (!(current == reportedBest_)) {
                reportedBest_ = current;
                pathDidChange = true;
                pathChanged = directConfig_->pathChanged;
                if (best) {
                    changedEndpoint = best->toString();
                    changedRtt = directPath_.bestRttMs();
                }
            }
            directActive_.store(best.has_value(), std::memory_order_release);
        }
        if (!callMeMaybe.empty())
            sendDiscoViaDerp(callMeMaybe);
        if (publish && !published.empty())
            publish(std::move(published));
        if (pathDidChange) {
            logTsRoute(VpnFileLogger::Severity::Info,
                       changedEndpoint.empty()
                           ? std::string("direct path lost; WireGuard is back "
                                         "on DERP")
                           : "WireGuard now goes directly to " +
                                 changedEndpoint + " (rtt " +
                                 std::to_string(changedRtt) +
                                 " ms) instead of DERP");
            if (pathChanged)
                pathChanged(changedEndpoint, changedRtt);
        }
    }

    void handleUdpPacket(const std::uint8_t* data, std::size_t length,
                         const IPv4Endpoint& from) {
        const std::span<const std::uint8_t> packet(data, length);
        if (handleSelfTest(packet, from))
            return;
        if (udpPacketsReceived_.fetch_add(1, std::memory_order_relaxed) == 0)
            logTsRoute(VpnFileLogger::Severity::Info,
                       "direct path: first UDP packet received (" +
                           std::to_string(length) + " bytes from " +
                           from.toString() + ")");
        if (stun::looksLikeStun(packet)) {
            stunReceived_.fetch_add(1, std::memory_order_relaxed);
            std::lock_guard lock(directMutex_);
            const auto mapped = stun::parseBindingResponse(packet, stunTx_);
            if (!mapped && !stunUnparsedLogged_) {
                stunUnparsedLogged_ = true;
                logTsRoute(VpnFileLogger::Severity::Warning,
                           "direct path: STUN reply from " + from.toString() +
                               " could not be used (" +
                               std::to_string(length) + " bytes)");
            }
            if (mapped && !(*mapped == stunMapped_)) {
                // A new public mapping replaces the old one.
                localEndpoints_.erase(
                    std::remove(localEndpoints_.begin(), localEndpoints_.end(),
                                stunMapped_),
                    localEndpoints_.end());
                stunMapped_ = *mapped;
                localEndpoints_.push_back(*mapped);
                endpointsChanged_ = true;
                logTsRoute(VpnFileLogger::Severity::Info,
                           "direct path: public endpoint " + mapped->toString() +
                               " (STUN)");
            }
            return;
        }
        if (disco::looksLikeDisco(packet)) {
            udpDiscoReceived_.fetch_add(1, std::memory_order_relaxed);
            handleDisco(packet, from);
            return;
        }
        const bool wgHeader = length >= 4 && data[0] >= 1 && data[0] <= 4 &&
                              data[1] == 0 && data[2] == 0 && data[3] == 0;
        if (!wgHeader || !context_) {
            udpOtherReceived_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        {
            // WireGuard authenticates every packet, but only accept it from
            // addresses this peer is known to use.
            std::lock_guard lock(directMutex_);
            if (!directPath_.isCandidate(from))
                return;
            if (!directWgLogged_) {
                directWgLogged_ = true;
                logTsRoute(VpnFileLogger::Severity::Info,
                           "direct path: receiving WireGuard from " +
                               from.toString());
            }
        }
        directReceived_.fetch_add(1, std::memory_order_relaxed);
        wgx_inject_encrypted(context_,
                             activePeerId_.load(std::memory_order_acquire),
                             data, length);
    }

    void udpLoop() {
        const auto stunServer = resolveStunServer();
        auto nextStun = DirectClock::now();
        auto nextCallMeMaybe = DirectClock::now();
        auto nextHousekeeping = DirectClock::now();
        std::vector<std::uint8_t> buffer(4096);
        sendSelfTests();
        while (udpRunning_) {
            const auto now = DirectClock::now();
            if (now >= nextHousekeeping) {
                directHousekeeping(now, stunServer, nextStun, nextCallMeMaybe);
                nextHousekeeping = now + std::chrono::milliseconds(100);
            }
            pollfd pfd{};
            pfd.fd = udpFd_;
            pfd.events = POLLIN;
            const int ready = ::poll(&pfd, 1, 100);
            if (ready < 0 || (ready > 0 && (pfd.revents & POLLNVAL))) {
                if (udpRecvFailures_.fetch_add(1, std::memory_order_relaxed) == 0)
                    logTsRoute(VpnFileLogger::Severity::Warning,
                               "direct path: UDP poll failed (rc=" +
                                   std::to_string(ready) + " revents=" +
                                   std::to_string(pfd.revents) + " errno " +
                                   std::to_string(errno) + ")");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            if (ready == 0 || !(pfd.revents & POLLIN))
                continue;
            sockaddr_in source{};
            socklen_t sourceLen = sizeof(source);
            const auto received = ::recvfrom(
                udpFd_, buffer.data(), buffer.size(), 0,
                reinterpret_cast<sockaddr*>(&source), &sourceLen);
            if (received <= 0) {
                if (udpRecvFailures_.fetch_add(1, std::memory_order_relaxed) == 0)
                    logTsRoute(VpnFileLogger::Severity::Warning,
                               "direct path: UDP receive failed (rc=" +
                                   std::to_string(received) + " errno " +
                                   std::to_string(errno) + ")");
                continue;
            }
            handleUdpPacket(buffer.data(), static_cast<std::size_t>(received),
                            fromSockaddr(source));
        }
    }

    // ---- Diagnostics (log text only; never change the packet path) ----

    static const char* wgTypeName(std::uint32_t type) {
        switch (type) {
        case 1: return "handshake initiation";
        case 2: return "handshake response";
        case 3: return "cookie reply";
        case 4: return "transport data";
        default: return "unknown";
        }
    }

    // Counts what the peer sends us, logging the first of each kind. Called
    // only from the DERP reader thread.
    void classifyIngress(const std::uint8_t* bytes, std::size_t length) {
        ingressBytes_.fetch_add(length, std::memory_order_relaxed);
        // Tailscale disco frames start with "TS" + U+1F4AC.
        static constexpr std::uint8_t kDiscoMagic[6] = {0x54, 0x53, 0xf0,
                                                        0x9f, 0x92, 0xac};
        if (length >= sizeof(kDiscoMagic) &&
            std::memcmp(bytes, kDiscoMagic, sizeof(kDiscoMagic)) == 0) {
            if (ingressDisco_.fetch_add(1, std::memory_order_relaxed) == 0)
                logTsRoute(VpnFileLogger::Severity::Info,
                           "peer sent a disco packet over DERP (its tailscaled "
                           "knows this node and is probing paths) t+" +
                               std::to_string(msSinceHandshakeStart()) + "ms");
            return;
        }
        const bool wgHeader = length >= 4 && bytes[0] >= 1 && bytes[0] <= 4 &&
                              bytes[1] == 0 && bytes[2] == 0 && bytes[3] == 0;
        if (!wgHeader) {
            if (ingressUnknown_.fetch_add(1, std::memory_order_relaxed) == 0)
                logTsRoute(VpnFileLogger::Severity::Warning,
                           "peer sent a packet that is neither WireGuard nor "
                           "disco (len=" + std::to_string(length) + ")");
            return;
        }
        const unsigned type = bytes[0];
        ingressCount_[type].fetch_add(1, std::memory_order_relaxed);
        if (!ingressTypesLogged_[type]) {
            ingressTypesLogged_[type] = true;
            logTsRoute(VpnFileLogger::Severity::Info,
                       "DERP relay received WireGuard type=" +
                           std::to_string(type) + " (" + wgTypeName(type) +
                           ") len=" + std::to_string(length) + " from peer t+" +
                           std::to_string(msSinceHandshakeStart()) + "ms");
        }
    }

    long long msSinceHandshakeStart() const {
        const auto start = handshakeStartMs_.load(std::memory_order_relaxed);
        if (start == 0)
            return -1;
        return nowMs() - start;
    }

    static long long nowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    void resetCounters() {
        for (auto& counter : egressCount_)
            counter.store(0, std::memory_order_relaxed);
        for (auto& counter : ingressCount_)
            counter.store(0, std::memory_order_relaxed);
        for (auto& seen : ingressTypesLogged_)
            seen = false;
        egressBytes_ = 0;
        ingressBytes_ = 0;
        egressDropped_ = 0;
        egressSendFailures_ = 0;
        ingressDisco_ = 0;
        ingressUnknown_ = 0;
        foreignPackets_ = 0;
        injectFailures_ = 0;
        maxInjectWaitMs_ = 0;
        slowInjectLogs_ = 0;
        summaryLogged_ = false;
    }

    std::string counterLine() const {
        auto load = [](const std::atomic_uint32_t& value) {
            return std::to_string(value.load(std::memory_order_relaxed));
        };
        return "sent: init=" + load(egressCount_[1]) +
               " resp=" + load(egressCount_[2]) +
               " cookie=" + load(egressCount_[3]) +
               " data=" + load(egressCount_[4]) +
               " bytes=" + std::to_string(egressBytes_.load()) +
               " dropped=" + load(egressDropped_) +
               " writeFail=" + load(egressSendFailures_) +
               " | received from peer: init=" + load(ingressCount_[1]) +
               " resp=" + load(ingressCount_[2]) +
               " cookie=" + load(ingressCount_[3]) +
               " data=" + load(ingressCount_[4]) +
               " disco=" + load(ingressDisco_) +
               " unknown=" + load(ingressUnknown_) +
               " bytes=" + std::to_string(ingressBytes_.load()) +
               " | from other nodes=" + load(foreignPackets_) +
               " | injectFail=" + load(injectFailures_) +
               " maxInjectWait=" + std::to_string(maxInjectWaitMs_.load()) +
               "ms | region=" + std::to_string(activeDerpRegion_.load()) +
               " peerHome=" + std::to_string(peerHomeRegion_.load());
    }

    void logTrafficSummary(const char* reason) {
        if (summaryLogged_.exchange(true))
            return;
        logTsRoute(VpnFileLogger::Severity::Info,
                   std::string("relay traffic summary (") + reason + "): " +
                       counterLine());
    }

    // Explains a handshake outcome from the counters, so the log states the
    // most likely cause instead of only "timed out".
    void logHandshakeDiagnosis(bool ok, long long elapsedMs) {
        logTsRoute(ok ? VpnFileLogger::Severity::Info
                      : VpnFileLogger::Severity::Error,
                   std::string("handshake ") + (ok ? "completed" : "FAILED") +
                       " after " + std::to_string(elapsedMs) + " ms; " +
                       counterLine());
        if (ok)
            return;
        auto count = [](const std::atomic_uint32_t& value) {
            return value.load(std::memory_order_relaxed);
        };
        std::string cause;
        if (count(egressCount_[1]) == 0) {
            cause = "WireGuard never emitted a handshake initiation, so "
                    "nothing reached DERP: the relay transport egress "
                    "callback is not wired or wgx_connect_peer failed early";
        } else if (count(egressSendFailures_) > 0 || !derpAlive_) {
            cause = "the DERP relay session failed during the handshake "
                    "(see 'DERP session ended' / 'SendPacket write failed')";
        } else if (count(ingressCount_[2]) > 0) {
            cause = "the peer's handshake response DID reach this Switch, "
                    "but WireGuard never marked the session valid. Max "
                    "inject wait was " +
                    std::to_string(maxInjectWaitMs_.load()) +
                    " ms: if that is seconds long, the response was held "
                    "back by the adapter mutex that wgx_connect_peer holds "
                    "for the whole handshake; if it is short, wg-nx rejected "
                    "the response (decrypt or index mismatch)";
        } else if (count(ingressCount_[3]) > 0) {
            cause = "the peer answered with a cookie reply (it is under load "
                    "and demands mac2); wg-nx must process cookie replies "
                    "and resend";
        } else if (count(ingressCount_[1]) > 0) {
            cause = "the peer's WireGuard is sending its OWN handshake "
                    "initiations to us, so its path to us works, but it never "
                    "answered ours: our initiation is being rejected (mac1 / "
                    "static key) or wg-nx is not answering inbound "
                    "initiations";
        } else if (count(ingressDisco_) > 0) {
            cause = "the peer's tailscaled sees us (disco pings arrive) but "
                    "WireGuard never answered: the peer dropped our "
                    "initiation (mac1 invalid, our nodekey not in its "
                    "netmap, or ACLs do not allow this node)";
        } else if (count(foreignPackets_) > 0) {
            cause = "DERP delivered packets from other nodes but none from "
                    "the target peer: it is not sending to our nodekey";
        } else {
            cause = "nothing at all came back from the peer over DERP. "
                    "Likely: the peer cannot address replies to us because "
                    "this node advertises no home DERP region (see "
                    "'self node: ... homeDERP=' above), the peer is offline, "
                    "or we are on DERP region " +
                    std::to_string(activeDerpRegion_.load()) +
                    " while the peer's home is " +
                    std::to_string(peerHomeRegion_.load());
        }
        logTsRoute(VpnFileLogger::Severity::Error,
                   "handshake diagnosis: " + cause);
    }

    bool startTunnel(const Key32& privateKey, const std::string& localIp,
                     std::string* error) override {
        stop();
        struct in_addr localAddr{};
        if (inet_pton(AF_INET, localIp.c_str(), &localAddr) != 1) {
            if (error) *error = "Invalid local IPv4 for wgx: " + localIp;
            return false;
        }

        context_ = wgx_create(privateKey.data(), localAddr.s_addr,
                              realEgressCallback, realIngressCallback, this);
        if (!context_) {
            if (error) *error = "wgx_create failed";
            return false;
        }

        localIp_ = localIp;
        tunnelCreated_ = true;
        return true;
    }

    bool addOrUpdatePeer(uint32_t peerId, const Key32& publicKey,
                         const std::string& peerIp, std::string* error) override {
        if (!context_ || !tunnelCreated_) {
            if (error) *error = "wgx tunnel is not created";
            return false;
        }
        struct in_addr peerAddr{};
        if (inet_pton(AF_INET, peerIp.c_str(), &peerAddr) != 1) {
            if (error) *error = "Invalid peer IPv4: " + peerIp;
            return false;
        }

        if (wgx_add_or_update_peer(context_, peerId, publicKey.data(),
                                   peerAddr.s_addr) != 0) {
            if (error) *error = "wgx_add_or_update_peer failed";
            return false;
        }

        // Publish the active peer BEFORE the blocking handshake below: replies
        // arriving mid-handshake must inject against this id, otherwise the
        // handshake can never complete and always burns the retry budget.
        activePeerId_ = peerId;
        activePeerIp_ = peerIp;

        // Start WireGuard tunnel now that peer is registered (peer_count > 0)
        if (!running_) {
            if (wgx_start(context_) != 0) {
                if (error) *error = "wgx_start failed";
                return false;
            }
            running_ = true;
        }

        // Blocking handshake over the relay established above: sub-second
        // when the peer is alive, bounded (~20s) when it is dead. A timeout
        // fails the route with a clear error instead of blackholing the
        // GameStream handshake that follows.
        logTsRoute(VpnFileLogger::Severity::Info,
                   "starting WireGuard handshake with peer " + peerIp +
                       " (up to 4 initiations, ~5 s each)");
        const long long handshakeStart = nowMs();
        handshakeStartMs_.store(handshakeStart, std::memory_order_relaxed);
        const int connectResult = wgx_connect_peer(context_, peerId);
        const long long handshakeElapsed = nowMs() - handshakeStart;
        if (connectResult != 0) {
            const std::string failure =
                "WireGuard handshake with peer timed out over DERP (rc=" +
                std::to_string(connectResult) + ")";
            if (error)
                *error = failure;
            logTsRoute(VpnFileLogger::Severity::Error, failure);
            logHandshakeDiagnosis(false, handshakeElapsed);
            return false;
        }
        logTsRoute(VpnFileLogger::Severity::Info,
                   "WireGuard handshake with peer completed");
        logHandshakeDiagnosis(true, handshakeElapsed);

        return true;
    }

    bool ensureDerpRoute(const Key32& peerNodeKey, int homeDerpRegion,
                         const std::vector<DerpRegion>& derpMap,
                         const Key32& localPrivateKey,
                         std::string* error) override {
        const bool peerKeyZero =
            std::all_of(peerNodeKey.begin(), peerNodeKey.end(),
                        [](std::uint8_t byte) { return byte == 0; });
        if (peerKeyZero) {
            if (error) *error = "DERP route requires a valid peer node key";
            return false;
        }
        const bool privateKeyZero =
            std::all_of(localPrivateKey.begin(), localPrivateKey.end(),
                        [](std::uint8_t byte) { return byte == 0; });
        if (privateKeyZero) {
            if (error)
                *error = "DERP route requires the local node identity (key is all zero)";
            return false;
        }
        if (homeDerpRegion <= 0) {
            if (error)
                *error = "peer has no home DERP region; the control-plane "
                         "netmap is incomplete";
            return false;
        }
        const DerpRegion* region = nullptr;
        for (const auto& candidate : derpMap) {
            if (candidate.regionId == homeDerpRegion) {
                region = &candidate;
                break;
            }
        }
        // Candidate regions: home first, then the rest of the map in order.
        // Official clients race regions by latency; pinning home-only strands
        // us on a far region while the tailnet's working traffic goes
        // elsewhere. Bounded so a dead map fails in seconds, not minutes.
        constexpr std::size_t kMaxFallbackRegions = 6;
        std::vector<const DerpRegion*> regions;
        regions.reserve(std::min<std::size_t>(derpMap.size(), kMaxFallbackRegions));
        if (region && !region->nodes.empty())
            regions.push_back(region);
        for (const auto& candidate : derpMap) {
            if (regions.size() >= kMaxFallbackRegions)
                break;
            if (region && candidate.regionId == region->regionId)
                continue;
            if (candidate.nodes.empty())
                continue;
            regions.push_back(&candidate);
        }
        if (regions.empty()) {
            if (error)
                *error = "DERP region " + std::to_string(homeDerpRegion) +
                         " is missing from the control-plane map and no "
                         "fallback region has nodes; cannot relay";
            return false;
        }

        // Already relaying for this peer: keep the live session instead of
        // flapping the relay on every route re-activation. The region does
        // not matter here; a relay connection serves every peer.
        if (derpAlive_ && derp_ && havePeer_ && peerNodeKey_ == peerNodeKey) {
            logTsRoute(VpnFileLogger::Severity::Info,
                       "reusing live DERP session on region " +
                           std::to_string(activeDerpRegion_.load()) + " via " +
                           activeDerpHost_);
            return true;
        }
        stopDerp();
        if (!region)
            logTsRoute(VpnFileLogger::Severity::Warning,
                       "peer home DERP region " +
                           std::to_string(homeDerpRegion) +
                           " is not in the control DERP map (" +
                           std::to_string(derpMap.size()) +
                           " regions); only fallback regions can be tried "
                           "and the peer will not see our packets there");

        Key32 localPublic{};
        tailscale_internal_crypto_x25519_public_key(localPublic.data(),
                                                    localPrivateKey.data());
        {
            // Identity proof: the pubkey we authenticate to DERP with must
            // match the node key control registered (admin console). If
            // these differ, the mesh drops our packets silently in both
            // directions and no handshake ever completes.
            constexpr char kHex[] = "0123456789abcdef";
            std::string localPrefix;
            for (int i = 0; i < 8; ++i) {
                localPrefix.push_back(kHex[localPublic[i] >> 4U]);
                localPrefix.push_back(kHex[localPublic[i] & 0x0fU]);
            }
            logTsRoute(VpnFileLogger::Severity::Info,
                       "DERP local nodekey=" + localPrefix + "...");
        }

        std::string lastError = "no DERP node attempted";
        for (const DerpRegion* tryRegion : regions) {
            logTsRoute(VpnFileLogger::Severity::Info,
                       "dialing DERP region " +
                           std::to_string(tryRegion->regionId) + " (" +
                           std::to_string(tryRegion->nodes.size()) +
                           " node(s)) for peer");
            for (const auto& node : tryRegion->nodes) {
            logTsRoute(VpnFileLogger::Severity::Info,
                       "DERP dialing " + node.host + ":" +
                           std::to_string(node.port));
            auto transport = std::make_unique<SwitchTlsTransport>();
            if (!transport->connect(node.host, node.port, &lastError)) {
                logTsRoute(VpnFileLogger::Severity::Warning,
                           "DERP TLS to " + node.host + " failed: " +
                               lastError);
                continue;
            }
            const std::string request = buildDerpUpgradeRequest(node.host);
            const std::span<const std::uint8_t> requestBytes(
                reinterpret_cast<const std::uint8_t*>(request.data()),
                request.size());
            if (!transport->write(requestBytes, &lastError)) {
                logTsRoute(VpnFileLogger::Severity::Warning,
                           "DERP upgrade write to " + node.host +
                               " failed: " + lastError);
                continue;
            }
            std::string header;
            if (!readDerpUpgradeHeader(*transport, &header, &lastError)) {
                logTsRoute(VpnFileLogger::Severity::Warning,
                           "DERP upgrade read from " + node.host +
                               " failed: " + lastError);
                continue;
            }
            if (!validateDerpUpgradeResponse(header, &lastError)) {
                logTsRoute(VpnFileLogger::Severity::Warning,
                           "DERP upgrade rejected by " + node.host + ": " +
                               lastError);
                continue;
            }
            auto session = std::make_unique<DerpSession>(
                std::move(transport), localPrivateKey, localPublic);
            if (!session->connect(&lastError)) {
                logTsRoute(VpnFileLogger::Severity::Warning,
                           "DERP auth with " + node.host +
                               " failed: " + lastError);
                continue;
            }
            resetCounters();
            handshakeStartMs_.store(0, std::memory_order_relaxed);
            derp_ = std::move(session);
            peerNodeKey_ = peerNodeKey;
            havePeer_ = true;
            activeDerpRegion_ = tryRegion->regionId;
            peerHomeRegion_ = homeDerpRegion;
            activeDerpHost_ = node.host;
            derpAlive_ = true;
            derpRunning_ = true;
            {
                // Mark this as our preferred (home) connection, matching the
                // region advertised to control as NetInfo.PreferredDERP.
                std::lock_guard lock(derpWriteMutex_);
                const std::uint8_t preferred = 1;
                std::string noteError;
                if (!derp_->writeRaw(DerpFrameType::NotePreferred,
                                     std::span<const std::uint8_t>(&preferred, 1),
                                     &noteError))
                    logTsRoute(VpnFileLogger::Severity::Warning,
                               "DERP NotePreferred not sent: " + noteError);
            }
            derpThread_ = std::thread(&RealWgxBackend::derpReader, this);
            logTsRoute(VpnFileLogger::Severity::Info,
                       "DERP relay connected via " + node.host + " region " +
                           std::to_string(tryRegion->regionId) +
                           " (peer home region " +
                           std::to_string(homeDerpRegion) + ")");
            if (tryRegion->regionId != homeDerpRegion)
                logTsRoute(
                    VpnFileLogger::Severity::Warning,
                    "connected to FALLBACK DERP region " +
                        std::to_string(tryRegion->regionId) +
                        ", not the peer's home region " +
                        std::to_string(homeDerpRegion) +
                        ": DERP only delivers between clients on the same "
                        "region, so the peer will not receive our packets "
                        "and cannot reply");
            // Self-test the session before trusting it with the handshake:
            // a Ping must come back as Pong within ~3s. This exercises the
            // read path with no peer, mesh, or filter involved. Advisory
            // only: a missing echo is logged, the route attempt continues.
            {
                const std::uint64_t token =
                    static_cast<std::uint64_t>(
                        std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count());
                std::string pingError;
                bool pingSent = false;
                {
                    std::lock_guard lock(derpWriteMutex_);
                    pingSent = derp_ && derp_->sendPing(token, &pingError);
                }
                if (!pingSent) {
                    logTsRoute(VpnFileLogger::Severity::Warning,
                               "DERP echo ping not sent: " + pingError);
                } else {
                    bool echoed = false;
                    for (int waited = 0; waited < 60; ++waited) {
                        if (lastPongToken_.load(std::memory_order_acquire) ==
                            token) {
                            echoed = true;
                            break;
                        }
                        if (!derpAlive_)
                            break;
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(50));
                    }
                    logTsRoute(
                        echoed ? VpnFileLogger::Severity::Info
                               : VpnFileLogger::Severity::Warning,
                        echoed ? "DERP echo OK" : "DERP echo: no Pong reply");
                }
            }
            // NOTE: no WatchConns subscription here. Frame 0x10 from a plain
            // client is a protocol violation: the relay closes the session
            // the instant it arrives, which used to masquerade as a peer
            // handshake timeout. PeerPresent/PeerGone below stay handled in
            // case the server volunteers any.
            return true;
            } // nodes in this region
            logTsRoute(VpnFileLogger::Severity::Warning,
                       "DERP region " + std::to_string(tryRegion->regionId) +
                           " failed: " + lastError);
        } // candidate regions
        const std::string failure =
            "DERP regions unreachable (" +
            std::to_string(regions.size()) + " tried): " + lastError;
        if (error)
            *error = failure;
        logTsRoute(VpnFileLogger::Severity::Error, failure);
        return false;
    }

    bool startTcpProxy(const std::string& peerIp, const std::string& hostIp,
                       std::span<const std::uint16_t> ports,
                       std::string* error) override {
        if (!context_ || !running_) {
            if (error) *error = "wgx tunnel is not running";
            return false;
        }

        if (relay_) {
            auto guard = SocketFdLock::instance().guard();
            relay_.reset();
        }

        // The relay lives inside the Tailscale wgx archive so its WireGuard
        // and lwIP calls bind to the Tailscale copy (see wgx_relay_shim.cpp).
        relay_.reset(wgx_relay_create(context_, tailscaleRelayLog));
        if (!relay_) {
            if (error) *error = "wgx_relay_create failed";
            logTsRoute(VpnFileLogger::Severity::Error,
                       "lwIP relay could not be created");
            return false;
        }

        auto guard = SocketFdLock::instance().guard();
        // For a subnet route the relay dials the LAN host, but every packet
        // is still sent to (and handshaken with) the subnet router's peer.
        const bool viaSubnetRouter = hostIp != peerIp;
        if (wgx_relay_start(relay_.get(), localIp_.c_str(), hostIp.c_str(),
                            peerIp.c_str()) != 0) {
            relay_.reset();
            if (error) *error = "LwipRelay start failed for " + hostIp;
            logTsRoute(VpnFileLogger::Severity::Error,
                       "lwIP relay start failed (local " + localIp_ +
                           " -> host " + hostIp + " via peer " + peerIp + ")");
            return false;
        }

        std::string portList;
        for (const auto port : ports) {
            if (wgx_relay_add_tcp(relay_.get(), port) != 0) {
                relay_.reset();
                if (error) *error = "Failed to start TCP relay port " + std::to_string(port);
                logTsRoute(VpnFileLogger::Severity::Error,
                           "TCP relay listener failed on 127.0.0.1:" +
                               std::to_string(port) +
                               " (port in use or socket budget exhausted)");
                return false;
            }
            if (!portList.empty())
                portList += ",";
            portList += std::to_string(port);
        }

        activePeerIp_ = peerIp;
        logTsRoute(VpnFileLogger::Severity::Info,
                   "TCP proxy ready: 127.0.0.1:{" + portList + "} -> " +
                       hostIp +
                       (viaSubnetRouter
                            ? " via subnet router " + peerIp
                            : std::string{}) +
                       " through the tunnel; " + counterLine());
        return true;
    }

    bool startUdpRelay(const std::string& peerIp,
                       std::span<const std::uint16_t> ports,
                       std::string* error) override {
        (void)peerIp;
        if (!relay_ || !wgx_relay_is_running(relay_.get())) {
            if (error) *error = "Cannot start UDP relays: TCP proxy is not running";
            return false;
        }
        if (udpPrepared_)
            return true;

        auto guard = SocketFdLock::instance().guard();
        for (const auto port : ports) {
            if (wgx_relay_add_udp(relay_.get(), port) != 0) {
                if (error) *error = "Failed to start UDP relay port " + std::to_string(port);
                logTsRoute(VpnFileLogger::Severity::Error,
                           "UDP relay failed on port " + std::to_string(port));
                return false;
            }
        }
        udpPrepared_ = true;
        logTsRoute(VpnFileLogger::Severity::Info,
                   "UDP media relays ready; traffic so far: " + counterLine());
        return true;
    }

    void stopUdpRelay() noexcept override {
        udpPrepared_ = false;
    }

    // DERP frame pump: the only thread that blocks in the relay transport.
    // RecvPackets from the active peer are decrypted by DERP framing already
    // (server-routed by node key) and handed to WireGuard as encrypted input.
    // Ping frames are echoed so the relay keeps this client marked present.
    // Any other frame type is protocol housekeeping and safely ignored here.
    void derpReader() {
        std::string error;
        bool firstPacketLogged = false;
        bool foreignLogged = false;
        bool shortLogged = false;
        while (derpRunning_) {
            auto* session = derp_.get();
            if (!session)
                break;
            auto frame = session->recvFrame(&error);
            if (!frame) {
                if (derpRunning_)
                    logTsRoute(VpnFileLogger::Severity::Warning,
                               "DERP session ended: " +
                                   (error.empty() ? "connection closed" : error));
                break;
            }
            switch (frame->type) {
            case DerpFrameType::RecvPacket: {
                const auto& payload = frame->payload;
                if (payload.size() <= kDerpKeyLen) {
                    if (!shortLogged) {
                        shortLogged = true;
                        logTsRoute(VpnFileLogger::Severity::Warning,
                                   "DERP relay sent a runt packet (" +
                                       std::to_string(payload.size()) +
                                       " bytes)");
                    }
                    break;
                }
                if (!context_ ||
                    std::memcmp(payload.data(), peerNodeKey_.data(),
                                kDerpKeyLen) != 0) {
                    // A packet from a node we are not talking to (stale
                    // session, wrong peer key) — never inject it. Log the
                    // source prefix so a run can prove whether the mesh is
                    // delivering anything at all.
                    foreignPackets_.fetch_add(1, std::memory_order_relaxed);
                    if (!foreignLogged) {
                        foreignLogged = true;
                        constexpr char kHex[] = "0123456789abcdef";
                        std::string srcPrefix;
                        for (int i = 0; i < 8; ++i) {
                            srcPrefix.push_back(kHex[payload[i] >> 4U]);
                            srcPrefix.push_back(kHex[payload[i] & 0x0fU]);
                        }
                        logTsRoute(VpnFileLogger::Severity::Warning,
                                   "DERP relay sent a packet from an unknown "
                                   "node " + srcPrefix + "...; ignoring");
                    }
                    break;
                }
                const auto* inner = payload.data() + kDerpKeyLen;
                const std::size_t innerLen = payload.size() - kDerpKeyLen;
                classifyIngress(inner, innerLen);
                // Disco is path discovery, not WireGuard: never inject it.
                if (disco::looksLikeDisco(
                        std::span<const std::uint8_t>(inner, innerLen))) {
                    handleDisco(std::span<const std::uint8_t>(inner, innerLen),
                                std::nullopt);
                    break;
                }
                const std::uint32_t injectPeer =
                    activePeerId_.load(std::memory_order_acquire);
                const long long injectStart = nowMs();
                const int injectResult = wgx_inject_encrypted(
                    context_, injectPeer, inner, innerLen);
                const long long injectWait = nowMs() - injectStart;
                if (injectWait > maxInjectWaitMs_.load(std::memory_order_relaxed))
                    maxInjectWaitMs_.store(injectWait, std::memory_order_relaxed);
                if (injectWait >= 250 && slowInjectLogs_.fetch_add(1) < 3)
                    logTsRoute(
                        VpnFileLogger::Severity::Warning,
                        "handing a peer packet (" +
                            std::string(innerLen >= 1 && inner[0] <= 4
                                            ? wgTypeName(inner[0])
                                            : "non-WireGuard") +
                            ") to WireGuard blocked for " +
                            std::to_string(injectWait) +
                            " ms: the wgx adapter mutex was held (normally by "
                            "the blocking handshake in wgx_connect_peer), so "
                            "this packet arrived too late to be useful");
                if (injectResult != 0 &&
                    injectFailures_.fetch_add(1, std::memory_order_relaxed) == 0)
                    logTsRoute(VpnFileLogger::Severity::Warning,
                               "WireGuard refused an injected peer packet (rc=" +
                                   std::to_string(injectResult) +
                                   " activePeerId=" +
                                   std::to_string(injectPeer) +
                                   (injectPeer == 0
                                        ? ": no active peer published yet)"
                                        : ")"));
                if (!firstPacketLogged) {
                    firstPacketLogged = true;
                    logTsRoute(VpnFileLogger::Severity::Info,
                               "DERP relay delivering peer packets");
                }
                break;
            }
            case DerpFrameType::Ping: {
                std::lock_guard lock(derpWriteMutex_);
                if (derpAlive_ && derp_)
                    derp_->writeRaw(DerpFrameType::Pong, frame->payload,
                                    nullptr);
                break;
            }
            case DerpFrameType::Pong: {
                // Echo reply to our own Ping (see ensureDerpRoute): proves
                // the relay session is bidirectional without involving any
                // peer, mesh hop, or filter.
                if (frame->payload.size() >= 8) {
                    std::uint64_t token = 0;
                    for (int i = 7; i >= 0; --i)
                        token = (token << 8U) | frame->payload[i];
                    lastPongToken_.store(token, std::memory_order_release);
                    if (!pongLogged_) {
                        pongLogged_ = true;
                        logTsRoute(VpnFileLogger::Severity::Info,
                                   "DERP echo reply received");
                    }
                }
                break;
            }
            case DerpFrameType::KeepAlive:
                break;
            case DerpFrameType::PeerPresent:
            case DerpFrameType::PeerGone: {
                // Presence traffic names 32-byte node keys; log a short
                // prefix so runs can be correlated with the admin console
                // without dumping full keys into the log.
                const auto& payload = frame->payload;
                std::string who = "unknown";
                if (payload.size() >= 32) {
                    constexpr char kHex[] = "0123456789abcdef";
                    who.clear();
                    for (int i = 0; i < 8; ++i) {
                        who.push_back(kHex[payload[i] >> 4U]);
                        who.push_back(kHex[payload[i] & 0x0fU]);
                    }
                }
                logTsRoute(VpnFileLogger::Severity::Info,
                           std::string(
                               frame->type == DerpFrameType::PeerPresent
                                   ? "DERP reports node present: "
                                   : "DERP reports node gone: ") +
                               who);
                break;
            }
            case DerpFrameType::Health: {
                // Empty body = healthy; text = the server's problem report.
                std::string text;
                for (const auto byte : frame->payload) {
                    if (text.size() >= 200)
                        break;
                    text.push_back(byte >= 0x20 && byte < 0x7f
                                       ? static_cast<char>(byte)
                                       : '.');
                }
                logTsRoute(text.empty() ? VpnFileLogger::Severity::Info
                                        : VpnFileLogger::Severity::Warning,
                           text.empty() ? "DERP server health: ok"
                                        : "DERP server health: " + text);
                break;
            }
            case DerpFrameType::Restarting:
                logTsRoute(VpnFileLogger::Severity::Warning,
                           "DERP server announced a restart; the relay "
                           "session will drop");
                break;
            default:
                break;
            }
        }
        // The relay is gone (or was never usable). Mark it dead so egress
        // drops fast and the next route activation reconnects instead of
        // stalling the WireGuard handshake into a UI hang.
        derpAlive_ = false;
        logTrafficSummary(derpRunning_ ? "relay session ended unexpectedly"
                                       : "relay stopped");
    }

    void stopDerp() noexcept {
        derpRunning_ = false;
        // Wake the reader thread first. Otherwise close() below waits for its
        // blocked TLS read, and Disconnect hangs until the relay's next
        // keepalive. No lock: interrupt only shuts the socket down.
        if (derp_)
            derp_->interrupt();
        {
            // Serialize against egress writes: closing the TLS connection
            // while a SendPacket write is in flight corrupts the session.
            std::lock_guard lock(derpWriteMutex_);
            if (derp_)
                derp_->close();
        }
        if (derpThread_.joinable())
            derpThread_.join();
        if (derp_)
            logTrafficSummary("relay stopped");
        // The reader is gone; reset under the write mutex so a concurrent
        // egress callback cannot observe half-torn relay state.
        std::lock_guard lock(derpWriteMutex_);
        derp_.reset();
        derpAlive_ = false;
        havePeer_ = false;
        egressLogged_ = false;
        pongLogged_ = false;
        initiationLogged_ = false;
        for (auto& seen : egressTypes_)
            seen = false;
        activeDerpRegion_ = 0;
        peerHomeRegion_ = 0;
        activeDerpHost_.clear();
    }

    void stop() noexcept override {
        stopDirectPath();
        stopDerp();
        running_ = false;
        if (relay_) {
            auto guard = SocketFdLock::instance().guard();
            relay_.reset();
        }
        if (context_) {
            wgx_destroy(context_);
            context_ = nullptr;
        }
        tunnelCreated_ = false;
        localIp_.clear();
        activePeerIp_.clear();
        activePeerId_ = 0;
        udpPrepared_ = false;
    }

    bool isRunning() const noexcept override {
        return tunnelCreated_ && context_ != nullptr;
    }

private:
    struct RelayDeleter {
        void operator()(WgxRelay* relay) const noexcept {
            wgx_relay_destroy(relay);
        }
    };
    WgxContext* context_ = nullptr;
    std::unique_ptr<WgxRelay, RelayDeleter> relay_;
    // Written under the route mutex (addOrUpdatePeer/stop), read by the DERP
    // pump thread: atomic so relay teardown never races packet injection.
    std::atomic_uint32_t activePeerId_{0};
    bool tunnelCreated_ = false;
    std::string localIp_;
    std::string activePeerIp_;
    std::atomic_bool running_ = false;
    bool udpPrepared_ = false;
    // DERP relay state. derpWriteMutex_ serializes TLS writes between the
    // WireGuard egress callback and Ping/Pong echoes; the reader thread is
    // the only code that blocks in recvFrame.
    std::unique_ptr<DerpSession> derp_;
    std::thread derpThread_;
    std::mutex derpWriteMutex_;
    std::atomic_bool derpRunning_{false};
    std::atomic_bool derpAlive_{false};
    std::atomic_uint64_t lastPongToken_{0};
    Key32 peerNodeKey_{};
    bool havePeer_ = false;
    bool egressLogged_ = false;
    bool pongLogged_ = false;
    bool initiationLogged_ = false;
    bool egressTypes_[8] = {false};
    std::atomic_int activeDerpRegion_{0};
    std::atomic_int peerHomeRegion_{0};
    std::string activeDerpHost_;
    // Diagnostics counters, reset per DERP session. Indexed by WireGuard
    // message type (1..4); slot 0 collects anything else.
    std::atomic_uint32_t egressCount_[8]{};
    std::atomic_uint32_t ingressCount_[8]{};
    bool ingressTypesLogged_[8] = {false}; // DERP reader thread only
    std::atomic_uint64_t egressBytes_{0};
    std::atomic_uint64_t ingressBytes_{0};
    std::atomic_uint32_t egressDropped_{0};
    std::atomic_uint32_t egressSendFailures_{0};
    std::atomic_uint32_t ingressDisco_{0};
    std::atomic_uint32_t ingressUnknown_{0};
    std::atomic_uint32_t foreignPackets_{0};
    std::atomic_uint32_t injectFailures_{0};
    std::atomic_int64_t maxInjectWaitMs_{0};
    std::atomic_uint32_t slowInjectLogs_{0};
    std::atomic_int64_t handshakeStartMs_{0};
    std::atomic_bool summaryLogged_{false};

    // Direct path state. directMutex_ guards everything below except the
    // atomics; it is never held while taking derpWriteMutex_. udpFd_ is set
    // before the UDP thread starts and closed only after it is joined.
    std::mutex directMutex_;
    std::optional<DirectPathConfig> directConfig_;
    DirectPath directPath_;
    Key32 localDiscoPublic_{};
    Key32 localNodePublic_{};
    int udpFd_ = -1;
    std::uint16_t udpPort_ = 0;
    std::thread udpThread_;
    std::atomic_bool udpRunning_{false};
    std::atomic_bool directActive_{false};
    IPv4Endpoint reportedBest_{};
    IPv4Endpoint stunMapped_{};
    std::vector<IPv4Endpoint> localEndpoints_;
    TxId stunTx_{};
    bool endpointsChanged_ = false;
    bool directWgLogged_ = false;
    bool firstPongLogged_ = false;
    std::atomic_uint64_t directSent_{0};
    std::atomic_uint64_t directReceived_{0};
    std::atomic_uint32_t discoRejected_{0};
    // Direct-path diagnostics, reset per startDirectPath.
    std::atomic_uint32_t pingsSent_{0};
    std::atomic_uint32_t udpSendFailures_{0};
    std::atomic_uint32_t stunSent_{0};
    std::atomic_uint32_t udpPacketsReceived_{0};
    std::atomic_uint32_t stunReceived_{0};
    std::atomic_uint32_t udpDiscoReceived_{0};
    std::atomic_uint32_t udpOtherReceived_{0};
    std::atomic_uint32_t udpRecvFailures_{0};
    std::atomic_uint32_t pingsFromPeerUdp_{0};
    std::atomic_uint32_t pingsFromPeerDerp_{0};
    std::atomic_uint32_t pongsFromPeer_{0};
    std::atomic_uint32_t pongsMatched_{0};
    std::atomic_uint32_t callMeMaybeFromPeer_{0};
    bool unmatchedPongLogged_ = false;  // directMutex_
    bool stunUnparsedLogged_ = false;   // directMutex_
    bool progressLogged_ = false;       // directMutex_
    DirectClock::time_point directStarted_{};
    // Self-tests (UDP thread only, except the flags read by the summary).
    std::array<std::uint8_t, 2> dnsId_{};
    DirectClock::time_point selfTestSent_{};
    std::atomic_bool loopbackEchoed_{false};
    std::atomic_bool dnsAnswered_{false};
    bool stunRequestLogged_ = false; // directMutex_
};
#endif

TailscaleWgxRoute::TailscaleWgxRoute(std::shared_ptr<IWgxBackend> backend,
                                     PeerResolver peerResolver,
                                     LocalInfoProvider localInfoProvider)
    : backend_(std::move(backend)), peerResolver_(std::move(peerResolver)),
      localInfoProvider_(std::move(localInfoProvider)) {
#if defined(__SWITCH__) && defined(ENABLE_TAILSCALE)
    if (!backend_) {
        backend_ = std::make_shared<RealWgxBackend>();
    }
#endif
}

void TailscaleWgxRoute::setPeerResolver(PeerResolver resolver) {
    std::lock_guard lock(mutex_);
    peerResolver_ = std::move(resolver);
}

void TailscaleWgxRoute::setLocalInfoProvider(LocalInfoProvider provider) {
    std::lock_guard lock(mutex_);
    localInfoProvider_ = std::move(provider);
}

void TailscaleWgxRoute::setDerpMapProvider(DerpMapProvider provider) {
    std::lock_guard lock(mutex_);
    derpMapProvider_ = std::move(provider);
}

void TailscaleWgxRoute::setBackend(std::shared_ptr<IWgxBackend> backend) {
    std::lock_guard lock(mutex_);
    backend_ = std::move(backend);
}

void TailscaleWgxRoute::setDirectPathHooks(DiscoKeyProvider discoKey,
                                           EndpointPublisher publisher,
                                           PathObserver observer) {
    std::lock_guard lock(mutex_);
    discoKeyProvider_ = std::move(discoKey);
    endpointPublisher_ = std::move(publisher);
    pathObserver_ = std::move(observer);
}

bool TailscaleWgxRoute::start(const RemoteRouteTarget& target,
                              std::string* error) {
    std::lock_guard lock(mutex_);
#if defined(__SWITCH__) && defined(ENABLE_TAILSCALE)
    logTsRoute(VpnFileLogger::Severity::Info,
               "route start: peer=" + target.peerId +
                   " addr=" + target.peerAddress + " host=" +
                   (target.targetAddress.empty() ? target.peerAddress
                                                 : target.targetAddress));
#endif
    if (!backend_) {
        if (error)
            *error =
                "Tailscale encrypted packet path is not release-ready; refusing "
                "to advertise a routable connection";
        return false;
    }

    if (target.peerAddress.empty() ||
        !PeerDirectory::isLiteralIPv4(target.peerAddress)) {
        if (error)
            *error = "Tailscale route target has no valid IPv4 address: " +
                     target.peerAddress;
        return false;
    }
    // The GameStream host. Equal to the peer for a tailnet address; a LAN
    // address behind the peer when the peer is a subnet router.
    const std::string hostIp = target.targetAddress.empty()
                                   ? target.peerAddress
                                   : target.targetAddress;
    if (!PeerDirectory::isLiteralIPv4(hostIp)) {
        if (error)
            *error = "Tailscale route host is not a valid IPv4 address: " +
                     hostIp;
        return false;
    }

    Key32 localPrivateKey{};
    std::string localIp;
    if (localInfoProvider_) {
        auto localInfo = localInfoProvider_();
        if (!localInfo) {
            if (error)
                *error = "Tailscale local identity is not available";
            return false;
        }
        localIp = localInfo->first;
        localPrivateKey = localInfo->second;
    } else {
        if (error)
            *error = "Tailscale local identity provider is not configured";
        return false;
    }

    Key32 peerKey{};
    int homeDerpRegion = 0;
    std::optional<Peer> resolvedPeer;
    if (peerResolver_) {
        auto peer = peerResolver_(target.peerId);
        resolvedPeer = peer;
        if (!peer) {
            if (error)
                *error = "Tailscale peer not found in netmap: " + target.peerId;
            return false;
        }
        const bool keyEmpty =
            std::all_of(peer->nodeKey.begin(), peer->nodeKey.end(),
                        [](std::uint8_t b) { return b == 0; });
        if (keyEmpty) {
            if (error)
                *error = "Tailscale peer has no valid node key: " + target.peerId;
            return false;
        }
        peerKey = peer->nodeKey;
        homeDerpRegion = peer->homeDerp;
#if defined(__SWITCH__) && defined(ENABLE_TAILSCALE)
        {
            constexpr char kHex[] = "0123456789abcdef";
            std::string keyPrefix;
            for (int i = 0; i < 8; ++i) {
                keyPrefix.push_back(kHex[peerKey[i] >> 4U]);
                keyPrefix.push_back(kHex[peerKey[i] & 0x0fU]);
            }
            logTsRoute(VpnFileLogger::Severity::Info,
                       "peer " + target.peerId + ": online=" +
                           (peer->online ? "yes" : "no") + " homeDerp=" +
                           std::to_string(peer->homeDerp) + " endpoints=" +
                           std::to_string(peer->endpoints.size()) +
                           " nodekey=" + keyPrefix + "...");
        }
#endif
    } else {
        if (error)
            *error = "Tailscale peer resolver is not configured";
        return false;
    }

    if (!backend_->isRunning()) {
        if (!backend_->startTunnel(localPrivateKey, localIp, error)) {
#if defined(__SWITCH__) && defined(ENABLE_TAILSCALE)
            logTsRoute(VpnFileLogger::Severity::Error,
                       "route step 1/4 failed: WireGuard tunnel create: " +
                           (error ? *error : std::string{}));
#endif
            return false;
        }
    }
#if defined(__SWITCH__) && defined(ENABLE_TAILSCALE)
    logTsRoute(VpnFileLogger::Severity::Info,
               "route step 1/4 ok: WireGuard tunnel up with local address " +
                   localIp);
#endif

    // The encrypted packet path must be relayed (DERP) BEFORE the WireGuard
    // handshake runs: wg_connect_peer blocks until the handshake completes,
    // and with no relay underneath it burns the whole retry budget (up to
    // 90s) holding the route mutex. Advertising a route without a working
    // relay is what used to blackhole GameStream handshakes into a hang.
    const std::vector<DerpRegion> derpMap =
        derpMapProvider_ ? derpMapProvider_() : std::vector<DerpRegion>{};
#if defined(__SWITCH__) && defined(ENABLE_TAILSCALE)
    logTsRoute(derpMap.empty() ? VpnFileLogger::Severity::Error
                               : VpnFileLogger::Severity::Info,
               "route step 2/4: DERP relay (control map has " +
                   std::to_string(derpMap.size()) + " regions, peer home " +
                   std::to_string(homeDerpRegion) + ")");
#endif
    if (!backend_->ensureDerpRoute(peerKey, homeDerpRegion, derpMap,
                                   localPrivateKey, error)) {
#if defined(__SWITCH__) && defined(ENABLE_TAILSCALE)
        logTsRoute(VpnFileLogger::Severity::Error,
                   "route step 2/4 failed: " +
                       (error ? *error : std::string{}));
#endif
        return false;
    }

    uint32_t peerNumId = static_cast<uint32_t>(
        std::hash<std::string>{}(target.peerId) & 0x7FFFFFFF);
    if (peerNumId == 0)
        peerNumId = 1;

#if defined(__SWITCH__) && defined(ENABLE_TAILSCALE)
    logTsRoute(VpnFileLogger::Severity::Info,
               "route step 3/4: WireGuard handshake");
#endif
    if (!backend_->addOrUpdatePeer(peerNumId, peerKey, target.peerAddress,
                                   error))
        return false;

#if defined(__SWITCH__) && defined(ENABLE_TAILSCALE)
    logTsRoute(VpnFileLogger::Severity::Info,
               "route step 4/4: TCP proxy for GameStream ports");
#endif
    if (!backend_->startTcpProxy(target.peerAddress, hostIp, kTailscaleTcpPorts,
                                 error))
        return false;

    // The route works over DERP now. Look for a direct UDP path alongside
    // it; that never fails or delays the route.
    const auto discoPrivate =
        discoKeyProvider_ ? discoKeyProvider_() : std::optional<Key32>{};
    const bool peerHasDisco =
        resolvedPeer &&
        std::any_of(resolvedPeer->discoKey.begin(), resolvedPeer->discoKey.end(),
                    [](std::uint8_t b) { return b != 0; });
    if (discoPrivate && peerHasDisco) {
        DirectPathConfig direct;
        direct.peerStableId = target.peerId;
        direct.peerNodeKey = peerKey;
        direct.peerDiscoKey = resolvedPeer->discoKey;
        for (const auto& endpoint : resolvedPeer->endpoints)
            direct.peerEndpoints.push_back(endpoint.address + ":" +
                                           std::to_string(endpoint.port));
        direct.localNodePrivate = localPrivateKey;
        direct.localDiscoPrivate = *discoPrivate;
        direct.derpMap = derpMap;
        direct.derpRegion = homeDerpRegion;
        direct.publishEndpoints = endpointPublisher_;
        if (pathObserver_) {
            direct.pathChanged = [observer = pathObserver_,
                                  peerId = target.peerId](
                                     const std::string& endpoint, int rttMs) {
                observer(peerId, endpoint, rttMs);
            };
        }
        backend_->startDirectPath(std::move(direct));
    }

    activeTarget_ = target;
    streamingPrepared_ = false;
    return true;
}

bool TailscaleWgxRoute::prepareForStreaming(const RemoteRouteTarget& target,
                                            std::string* error) {
    std::lock_guard lock(mutex_);
    if (!activeTarget_ || activeTarget_->peerId != target.peerId) {
        if (error)
            *error = "Cannot prepare streaming: route is not active for peer " +
                     target.peerId;
        return false;
    }
    if (!backend_ || !backend_->isRunning()) {
        if (error)
            *error = "Cannot prepare streaming: tunnel backend is not running";
        return false;
    }
    if (!backend_->startUdpRelay(target.peerAddress, kTailscaleUdpPorts, error))
        return false;

    streamingPrepared_ = true;
    return true;
}

void TailscaleWgxRoute::stop() noexcept {
    std::lock_guard lock(mutex_);
    if (backend_)
        backend_->stop();
    activeTarget_.reset();
    streamingPrepared_ = false;
}

bool TailscaleWgxRoute::isActive() const noexcept {
    std::lock_guard lock(mutex_);
    return activeTarget_.has_value() && backend_ && backend_->isRunning();
}

bool TailscaleWgxRoute::isStreamingPrepared() const noexcept {
    std::lock_guard lock(mutex_);
    return streamingPrepared_;
}

std::string TailscaleWgxRoute::activePeerId() const {
    std::lock_guard lock(mutex_);
    return activeTarget_ ? activeTarget_->peerId : std::string{};
}

} // namespace artemis::tailscale
