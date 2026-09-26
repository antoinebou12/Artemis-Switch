#pragma once

// Loopback TCP/UDP proxy for the Tailscale tunnel: 127.0.0.1:<port> on the
// Switch <-> <peer tailnet IP>:<port> through lwIP and wg-nx.
//
// Built ONLY into libtailscale-wgx.a (cmake/TailscaleWgxBackend.cmake), where
// every symbol is renamed to tailscale_internal_*, so it always runs on the
// Tailscale copy of wg-nx/lwIP. Not part of the app's own sources.
//
// This is a close port of wg-nx's wgnx::LwipRelay, whose socket handling is
// known to work on Switch (it carried the first four pairing requests).
// Deliberate differences, nothing else:
//  * lwIP -> client forwards the WHOLE pbuf chain. Upstream sent only the
//    first segment of a chain, so any host reply larger than one TCP segment
//    (the TLS ServerHello + certificate on 47984, the app list, ...) was
//    truncated and the client hung.
//  * UDP host -> client copies the full chain (reassembled fragments).
//  * one log line per connection event, with byte counts.

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

extern "C" {
#include "lwip/netif.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "wireguard.h"
#include "wg_netif.h"
#include <netinet/in.h>
#include <poll.h>
}

namespace artemis_tsrelay {

// level: 0 debug, 1 info, 2 error
using LogFn = std::function<void(int level, const char* message)>;

class TsLwipRelay {
public:
    TsLwipRelay(WgTunnel* tunnel, LogFn log);
    ~TsLwipRelay();

    TsLwipRelay(const TsLwipRelay&) = delete;
    TsLwipRelay& operator=(const TsLwipRelay&) = delete;

    // viaPeerIp: tailnet address of the WireGuard peer carrying targetIp.
    // Null/empty or equal to targetIp for a directly addressed peer; a subnet
    // router's address when targetIp is a LAN host behind it.
    bool start(const char* tunnelIp, const char* targetIp,
               const char* viaPeerIp = nullptr);
    void stop();
    bool addTcp(std::uint16_t port);
    bool addUdp(std::uint16_t port);
    bool isRunning() const { return running_; }

private:
    struct TcpConnection {
        TsLwipRelay* owner = nullptr;
        unsigned id = 0;
        int localSock = -1;
        tcp_pcb* pcb = nullptr;
        std::uint16_t port = 0;
        bool connected = false;
        // Set from lwIP callbacks; the client socket is closed only in
        // pollTcpConnections so an fd number is never reused while it is
        // still a key in tcpConnections_.
        bool hostDone = false;
        bool firstDownLogged = false;
        std::uint64_t up = 0;
        std::uint64_t down = 0;
        std::uint64_t startMs = 0;
    };

    struct UdpBinding {
        TsLwipRelay* owner = nullptr;
        int localSock = -1;
        udp_pcb* pcb = nullptr;
        std::uint16_t port = 0;
        sockaddr_in clientAddr{};
        bool hasClient = false;
        bool upLogged = false;
        bool downLogged = false;
    };

    struct IncomingSlot {
        WgRecvSlot* slot;
        const std::uint8_t* data;
        std::size_t len;
    };

    void log(int level, const char* fmt, ...);
    void runLoop();
    void signalWake();
    void processIncomingQueue();
    void pollTcpListeners();
    void pollTcpConnections();
    void pollUdpSockets();
    void logClosed(const TcpConnection& conn, const char* reason);

    int handleIncomingPacket(WgRecvSlot* slot, const void* data, std::size_t len);
    static int onTunnelRecv(void* user, WgRecvSlot* slot, const void* data,
                            std::size_t len);
    static err_t onTcpConnected(void* arg, tcp_pcb* pcb, err_t err);
    static err_t onTcpRecv(void* arg, tcp_pcb* pcb, pbuf* p, err_t err);
    static void onTcpError(void* arg, err_t err);
    static void onUdpRecv(void* arg, udp_pcb* pcb, pbuf* p,
                          const ip_addr_t* addr, u16_t port);

    // `base` must stay the first member: lwIP hands back netif*, which is
    // cast to RelayNetif* to find the owning relay.
    struct RelayNetif {
        netif base;
        TsLwipRelay* owner;
    };
    static err_t onNetifOutput(netif* nif, pbuf* p, const ip4_addr_t* dest);

    WgTunnel* tunnel_;
    LogFn log_;
    RelayNetif netif_{};
    netif_output_fn wgOutput_ = nullptr;
    ip4_addr_t tunnelAddr_{};
    ip4_addr_t targetAddr_{};
    ip4_addr_t viaAddr_{};
    bool viaSubnetRouter_ = false;

    std::atomic<bool> running_{false};
    bool initialized_ = false;
    std::thread loopThread_;
    std::mutex mutex_;

    std::map<std::uint16_t, int> tcpListeners_;
    std::map<int, std::shared_ptr<TcpConnection>> tcpConnections_;
    std::map<std::uint16_t, std::shared_ptr<UdpBinding>> udpBindings_;
    unsigned nextConnId_ = 1;

    std::queue<IncomingSlot> incomingQueue_;
    std::mutex queueMutex_;
    static constexpr std::size_t kSlotHolderCount = 128;
    std::array<WgSlotPbuf, kSlotHolderCount> slotHolders_{};

    int wakeFd_[2] = {-1, -1};
    std::vector<pollfd> pollFds_;
};

} // namespace artemis_tsrelay
