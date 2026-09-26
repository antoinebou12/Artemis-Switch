#include "TsLwipRelay.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

extern "C" {
#include "lwip/init.h"
#include "lwip/ip.h"
#include "lwip/pbuf.h"
#include "lwip/timeouts.h"
}

// Socket usage below intentionally mirrors wgnx::LwipRelay call for call:
// poll() gates every accept()/recv(), sockets close as soon as a connection
// ends. An earlier rewrite that accepted/received speculatively and deferred
// close() broke the Switch socket table (EBADF on curl's and the relay's own
// sockets), so do not "optimise" this without testing on hardware.

namespace artemis_tsrelay {
namespace {

std::uint64_t nowMs() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000ULL +
           static_cast<std::uint64_t>(ts.tv_nsec) / 1000000ULL;
}

const char* lwipErrorName(int err) {
    switch (err) {
    case ERR_OK: return "ok";
    case ERR_MEM: return "out of memory";
    case ERR_TIMEOUT: return "timeout";
    case ERR_RTE: return "no route";
    case ERR_CONN: return "not connected";
    case ERR_ABRT: return "aborted (retries exhausted, host unreachable)";
    case ERR_RST: return "reset by host (port closed / refused)";
    case ERR_CLSD: return "closed";
    default: return "error";
    }
}

int createListener(int type, std::uint16_t port) {
    const int sock = socket(AF_INET, type, 0);
    if (sock < 0)
        return -1;
    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (type == SOCK_DGRAM) {
        int rcvbuf = 0x19000;
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
        (type == SOCK_STREAM && listen(sock, 5) < 0)) {
        close(sock);
        return -1;
    }
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
    return sock;
}

// Sends all of buf on a non-blocking socket, briefly waiting for room.
bool sendAll(int sock, const std::uint8_t* buf, std::size_t len) {
    std::size_t done = 0;
    int stalls = 0;
    while (done < len) {
        const ssize_t sent = send(sock, buf + done, len - done, 0);
        if (sent > 0) {
            done += static_cast<std::size_t>(sent);
            stalls = 0;
            continue;
        }
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) &&
            ++stalls < 200) {
            pollfd pfd{sock, POLLOUT, 0};
            poll(&pfd, 1, 10);
            continue;
        }
        return false;
    }
    return true;
}

} // namespace

TsLwipRelay::TsLwipRelay(WgTunnel* tunnel, LogFn log)
    : tunnel_(tunnel), log_(std::move(log)) {
    for (auto& holder : slotHolders_) {
        holder.in_use = 0;
        holder.tunnel = nullptr;
        holder.slot = nullptr;
    }
}

TsLwipRelay::~TsLwipRelay() { stop(); }

void TsLwipRelay::log(int level, const char* fmt, ...) {
    if (!log_ || level == 0)
        return;
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    log_(level, buffer);
}

bool TsLwipRelay::start(const char* tunnelIp, const char* targetIp) {
    std::lock_guard lock(mutex_);
    if (running_ || !tunnel_)
        return false;
    if (!initialized_) {
        lwip_init();
        initialized_ = true;
    }
    if (inet_pton(AF_INET, tunnelIp, &tunnelAddr_) != 1 ||
        inet_pton(AF_INET, targetIp, &targetAddr_) != 1) {
        log(2, "invalid address (local %s, host %s)", tunnelIp, targetIp);
        return false;
    }

    ip4_addr_t netmask;
    ip4_addr_t gateway;
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gateway, 0, 0, 0, 0);
    netif_add(&netif_, &tunnelAddr_, &netmask, &gateway, tunnel_,
              wg_netif_init, ip_input);
    netif_set_default(&netif_);
    netif_set_up(&netif_);

    wakeFd_[0] = socket(AF_INET, SOCK_DGRAM, 0);
    wakeFd_[1] = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in wakeAddr{};
    wakeAddr.sin_family = AF_INET;
    wakeAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t wakeLen = sizeof(wakeAddr);
    if (wakeFd_[0] < 0 || wakeFd_[1] < 0 ||
        bind(wakeFd_[0], reinterpret_cast<sockaddr*>(&wakeAddr),
             sizeof(wakeAddr)) < 0 ||
        getsockname(wakeFd_[0], reinterpret_cast<sockaddr*>(&wakeAddr),
                    &wakeLen) < 0 ||
        connect(wakeFd_[1], reinterpret_cast<sockaddr*>(&wakeAddr),
                sizeof(wakeAddr)) < 0) {
        log(2, "wake socket setup failed (errno %d)", errno);
        for (auto& fd : wakeFd_) {
            if (fd >= 0) close(fd);
            fd = -1;
        }
        netif_set_down(&netif_);
        netif_remove(&netif_);
        return false;
    }
    fcntl(wakeFd_[0], F_SETFL, fcntl(wakeFd_[0], F_GETFL, 0) | O_NONBLOCK);
    fcntl(wakeFd_[1], F_SETFL, fcntl(wakeFd_[1], F_GETFL, 0) | O_NONBLOCK);
    pollFds_.reserve(32);

    running_ = true;
    wg_set_recv_callback(tunnel_, &TsLwipRelay::onTunnelRecv, this);
    loopThread_ = std::thread(&TsLwipRelay::runLoop, this);
    log(1, "started (relay rev 4, upstream socket model): local %s <-> host %s",
        tunnelIp, targetIp);
    return true;
}

void TsLwipRelay::signalWake() {
    if (wakeFd_[1] < 0)
        return;
    const char byte = 1;
    (void)write(wakeFd_[1], &byte, 1);
}

void TsLwipRelay::stop() {
    const bool wasRunning = running_.exchange(false);
    if (tunnel_)
        wg_set_recv_callback(tunnel_, nullptr, nullptr);
    signalWake();
    if (loopThread_.joinable())
        loopThread_.join();
    for (auto& fd : wakeFd_) {
        if (fd >= 0) close(fd);
        fd = -1;
    }
    {
        std::lock_guard qlock(queueMutex_);
        while (!incomingQueue_.empty()) {
            if (tunnel_ && incomingQueue_.front().slot)
                wg_recv_slot_release(tunnel_, incomingQueue_.front().slot);
            incomingQueue_.pop();
        }
    }

    std::lock_guard lock(mutex_);
    for (auto& [port, sock] : tcpListeners_)
        close(sock);
    tcpListeners_.clear();
    for (auto& [sock, conn] : tcpConnections_) {
        if (conn->localSock >= 0)
            close(conn->localSock);
        if (conn->pcb) {
            tcp_arg(conn->pcb, nullptr);
            tcp_abort(conn->pcb);
        }
    }
    tcpConnections_.clear();
    for (auto& [port, binding] : udpBindings_) {
        if (binding->localSock >= 0)
            close(binding->localSock);
        if (binding->pcb)
            udp_remove(binding->pcb);
    }
    udpBindings_.clear();
    if (netif_.flags & NETIF_FLAG_UP) {
        netif_set_down(&netif_);
        netif_remove(&netif_);
    }
    if (wasRunning)
        log(1, "stopped");
}

bool TsLwipRelay::addTcp(std::uint16_t port) {
    std::lock_guard lock(mutex_);
    if (tcpListeners_.count(port))
        return true;
    const int sock = createListener(SOCK_STREAM, port);
    if (sock < 0) {
        log(2, "tcp %u: listen on 127.0.0.1 failed (errno %d)", port, errno);
        return false;
    }
    tcpListeners_[port] = sock;
    log(1, "tcp %u: listening on 127.0.0.1", port);
    signalWake();
    return true;
}

bool TsLwipRelay::addUdp(std::uint16_t port) {
    std::lock_guard lock(mutex_);
    if (udpBindings_.count(port))
        return true;
    const int sock = createListener(SOCK_DGRAM, port);
    if (sock < 0) {
        log(2, "udp %u: bind on 127.0.0.1 failed (errno %d)", port, errno);
        return false;
    }
    udp_pcb* pcb = udp_new();
    if (!pcb) {
        close(sock);
        return false;
    }
    ip_addr_t local;
    ip_addr_copy_from_ip4(local, tunnelAddr_);
    if (udp_bind(pcb, &local, port) != ERR_OK) {
        udp_remove(pcb);
        close(sock);
        log(2, "udp %u: lwIP bind failed", port);
        return false;
    }
    auto binding = std::make_shared<UdpBinding>();
    binding->owner = this;
    binding->localSock = sock;
    binding->pcb = pcb;
    binding->port = port;
    udp_recv(pcb, onUdpRecv, binding.get());
    udpBindings_[port] = binding;
    log(1, "udp %u: relaying", port);
    signalWake();
    return true;
}

int TsLwipRelay::onTunnelRecv(void* user, WgRecvSlot* slot, const void* data,
                              std::size_t len) {
    return static_cast<TsLwipRelay*>(user)->handleIncomingPacket(slot, data, len);
}

int TsLwipRelay::handleIncomingPacket(WgRecvSlot* slot, const void* data,
                                      std::size_t len) {
    if (!running_ || !slot || !data || len == 0)
        return 0;
    bool wasEmpty;
    {
        std::lock_guard lock(queueMutex_);
        wasEmpty = incomingQueue_.empty();
        incomingQueue_.push({slot, static_cast<const std::uint8_t*>(data), len});
    }
    if (wasEmpty)
        signalWake();
    return 1;
}

void TsLwipRelay::processIncomingQueue() {
    constexpr std::size_t kMaxPerIteration = 16;
    std::vector<IncomingSlot> packets;
    bool more = false;
    {
        std::lock_guard lock(queueMutex_);
        const std::size_t n = std::min(incomingQueue_.size(), kMaxPerIteration);
        packets.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            packets.push_back(incomingQueue_.front());
            incomingQueue_.pop();
        }
        more = !incomingQueue_.empty();
    }
    for (const auto& packet : packets) {
        WgSlotPbuf* holder = nullptr;
        for (auto& candidate : slotHolders_) {
            if (!candidate.in_use) {
                holder = &candidate;
                break;
            }
        }
        if (!holder) {
            wg_recv_slot_release(tunnel_, packet.slot);
            continue;
        }
        wg_netif_input_slot(&netif_, tunnel_, packet.slot, packet.data,
                            packet.len, holder);
    }
    if (more)
        signalWake();
}

void TsLwipRelay::runLoop() {
    log(1, "loop started");
    constexpr int kPollTimeoutMs = 200;
    while (running_) {
        pollFds_.clear();
        pollFds_.push_back({wakeFd_[0], POLLIN, 0});
        {
            std::lock_guard lock(mutex_);
            for (auto& [port, sock] : tcpListeners_)
                pollFds_.push_back({sock, POLLIN, 0});
            for (auto& [sock, conn] : tcpConnections_) {
                if (conn->connected)
                    pollFds_.push_back({sock, POLLIN, 0});
            }
            for (auto& [port, binding] : udpBindings_)
                pollFds_.push_back({binding->localSock, POLLIN, 0});
        }
        poll(pollFds_.data(), pollFds_.size(), kPollTimeoutMs);
        if (!running_)
            break;
        if (pollFds_[0].revents & POLLIN) {
            char drain[64];
            while (read(wakeFd_[0], drain, sizeof(drain)) > 0) {
            }
        }
        std::lock_guard lock(mutex_);
        processIncomingQueue();
        pollTcpListeners();
        pollTcpConnections();
        pollUdpSockets();
        sys_check_timeouts();
    }
    log(1, "loop ended");
}

void TsLwipRelay::pollTcpListeners() {
    for (auto& [port, listenSock] : tcpListeners_) {
        pollfd pfd{listenSock, POLLIN, 0};
        if (poll(&pfd, 1, 0) <= 0 || !(pfd.revents & POLLIN))
            continue;
        sockaddr_in client{};
        socklen_t clientLen = sizeof(client);
        const int clientSock =
            accept(listenSock, reinterpret_cast<sockaddr*>(&client), &clientLen);
        if (clientSock < 0)
            continue;
        fcntl(clientSock, F_SETFL, fcntl(clientSock, F_GETFL, 0) | O_NONBLOCK);

        tcp_pcb* pcb = tcp_new();
        if (!pcb) {
            log(2, "tcp %u: no lwIP TCP PCB left; refusing client", port);
            close(clientSock);
            continue;
        }
        auto conn = std::make_shared<TcpConnection>();
        conn->owner = this;
        conn->id = nextConnId_++;
        conn->localSock = clientSock;
        conn->pcb = pcb;
        conn->port = port;
        conn->startMs = nowMs();
        tcp_arg(pcb, conn.get());
        tcp_err(pcb, onTcpError);
        tcpConnections_[clientSock] = conn;

        ip_addr_t dest;
        ip_addr_copy_from_ip4(dest, targetAddr_);
        log(1, "tcp #%u port %u: client accepted, connecting to host", conn->id,
            port);
        const err_t err = tcp_connect(pcb, &dest, port, onTcpConnected);
        if (err != ERR_OK) {
            log(2, "tcp #%u port %u: tcp_connect failed (%s)", conn->id, port,
                lwipErrorName(err));
            tcp_arg(pcb, nullptr);
            tcp_abort(pcb);
            close(clientSock);
            tcpConnections_.erase(clientSock);
        }
    }
}

void TsLwipRelay::logClosed(const TcpConnection& conn, const char* reason) {
    log(1, "tcp #%u port %u: %s after %llu ms (up=%llu down=%llu bytes)",
        conn.id, conn.port, reason,
        static_cast<unsigned long long>(nowMs() - conn.startMs),
        static_cast<unsigned long long>(conn.up),
        static_cast<unsigned long long>(conn.down));
}

void TsLwipRelay::pollTcpConnections() {
    std::vector<int> toRemove;
    for (auto& [sock, conn] : tcpConnections_) {
        if (conn->hostDone) {
            // Host side closed or failed (see onTcpRecv / onTcpError).
            toRemove.push_back(sock);
            continue;
        }
        if (!conn->connected)
            continue;
        pollfd pfd{sock, POLLIN, 0};
        const int ret = poll(&pfd, 1, 0);
        if (ret < 0) {
            logClosed(*conn, "client socket poll failed");
            toRemove.push_back(sock);
            continue;
        }
        if (ret > 0 && (pfd.revents & POLLIN)) {
            std::uint8_t buf[2048];
            const ssize_t received = recv(sock, buf, sizeof(buf), 0);
            if (received <= 0) {
                if (received == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
                    logClosed(*conn, received == 0 ? "closed by client"
                                                   : "client socket read failed");
                    if (conn->pcb) {
                        tcp_arg(conn->pcb, nullptr);
                        if (tcp_close(conn->pcb) != ERR_OK)
                            tcp_abort(conn->pcb);
                        conn->pcb = nullptr;
                    }
                    toRemove.push_back(sock);
                }
                continue;
            }
            if (conn->pcb) {
                const err_t err = tcp_write(conn->pcb, buf,
                                            static_cast<u16_t>(received),
                                            TCP_WRITE_FLAG_COPY);
                if (err == ERR_OK) {
                    tcp_output(conn->pcb);
                    conn->up += static_cast<std::uint64_t>(received);
                } else {
                    log(2, "tcp #%u port %u: tcp_write failed (%s)", conn->id,
                        conn->port, lwipErrorName(err));
                }
            }
        }
        if (pfd.revents & (POLLERR | POLLHUP)) {
            logClosed(*conn, "client socket hung up");
            toRemove.push_back(sock);
        }
    }
    for (const int sock : toRemove) {
        auto it = tcpConnections_.find(sock);
        if (it == tcpConnections_.end())
            continue;
        if (it->second->localSock >= 0)
            close(it->second->localSock);
        if (it->second->pcb) {
            tcp_arg(it->second->pcb, nullptr);
            tcp_abort(it->second->pcb);
        }
        tcpConnections_.erase(it);
    }
}

void TsLwipRelay::pollUdpSockets() {
    for (auto& [port, binding] : udpBindings_) {
        pollfd pfd{binding->localSock, POLLIN, 0};
        if (poll(&pfd, 1, 0) <= 0 || !(pfd.revents & POLLIN))
            continue;
        std::uint8_t buf[2048];
        sockaddr_in from{};
        socklen_t fromLen = sizeof(from);
        const ssize_t received =
            recvfrom(binding->localSock, buf, sizeof(buf), 0,
                     reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (received <= 0)
            continue;
        binding->clientAddr = from;
        binding->hasClient = true;
        pbuf* p = pbuf_alloc(PBUF_TRANSPORT, static_cast<u16_t>(received),
                             PBUF_RAM);
        if (!p)
            continue;
        std::memcpy(p->payload, buf, static_cast<std::size_t>(received));
        ip_addr_t dest;
        ip_addr_copy_from_ip4(dest, targetAddr_);
        udp_sendto(binding->pcb, p, &dest, binding->port);
        pbuf_free(p);
        if (!binding->upLogged) {
            binding->upLogged = true;
            log(1, "udp %u: first packet to host (%d bytes)", port,
                static_cast<int>(received));
        }
    }
}

err_t TsLwipRelay::onTcpConnected(void* arg, tcp_pcb* pcb, err_t err) {
    auto* conn = static_cast<TcpConnection*>(arg);
    if (!conn)
        return ERR_ARG;
    if (err != ERR_OK)
        return err;
    conn->connected = true;
    tcp_recv(pcb, onTcpRecv);
    conn->owner->log(1, "tcp #%u port %u: connected to host in %llu ms",
                     conn->id, conn->port,
                     static_cast<unsigned long long>(nowMs() - conn->startMs));
    return ERR_OK;
}

err_t TsLwipRelay::onTcpRecv(void* arg, tcp_pcb* pcb, pbuf* p, err_t err) {
    auto* conn = static_cast<TcpConnection*>(arg);
    if (!conn) {
        if (p) {
            tcp_recved(pcb, p->tot_len);
            pbuf_free(p);
        }
        return ERR_OK;
    }
    if (!p || err != ERR_OK) {
        // Host closed: close the client socket; pollTcpConnections reaps it.
        conn->owner->logClosed(*conn, "closed by host");
        conn->hostDone = true;
        if (p)
            pbuf_free(p);
        return ERR_OK;
    }
    // Forward the WHOLE chain. Upstream sent only p->payload/p->len, dropping
    // every later segment of a multi-segment reply (TLS handshakes, app list).
    bool ok = true;
    for (pbuf* q = p; q && ok; q = q->next)
        ok = sendAll(conn->localSock, static_cast<const std::uint8_t*>(q->payload),
                     q->len);
    conn->down += p->tot_len;
    if (!conn->firstDownLogged) {
        conn->firstDownLogged = true;
        conn->owner->log(1, "tcp #%u port %u: first %u bytes from host after %llu ms",
                         conn->id, conn->port, static_cast<unsigned>(p->tot_len),
                         static_cast<unsigned long long>(nowMs() - conn->startMs));
    }
    if (!ok)
        conn->owner->log(2, "tcp #%u port %u: writing host data to client failed "
                            "(errno %d)",
                         conn->id, conn->port, errno);
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

void TsLwipRelay::onTcpError(void* arg, err_t err) {
    auto* conn = static_cast<TcpConnection*>(arg);
    if (!conn)
        return;
    // lwIP has already freed the PCB.
    conn->pcb = nullptr;
    conn->owner->log(2, "tcp #%u port %u: %s: %s after %llu ms (up=%llu down=%llu)",
                     conn->id, conn->port,
                     conn->connected ? "connection lost" : "connect failed",
                     lwipErrorName(err),
                     static_cast<unsigned long long>(nowMs() - conn->startMs),
                     static_cast<unsigned long long>(conn->up),
                     static_cast<unsigned long long>(conn->down));
    conn->hostDone = true;
}

void TsLwipRelay::onUdpRecv(void* arg, udp_pcb*, pbuf* p, const ip_addr_t*,
                            u16_t) {
    auto* binding = static_cast<UdpBinding*>(arg);
    if (!p)
        return;
    if (!binding || !binding->hasClient || p->tot_len > 2048) {
        pbuf_free(p);
        return;
    }
    std::uint8_t buf[2048];
    const u16_t len = pbuf_copy_partial(p, buf, p->tot_len, 0);
    pbuf_free(p);
    sendto(binding->localSock, buf, len, 0,
           reinterpret_cast<sockaddr*>(&binding->clientAddr),
           sizeof(binding->clientAddr));
    if (!binding->downLogged) {
        binding->downLogged = true;
        binding->owner->log(1, "udp %u: first packet from host (%u bytes)",
                            binding->port, static_cast<unsigned>(len));
    }
}

} // namespace artemis_tsrelay
