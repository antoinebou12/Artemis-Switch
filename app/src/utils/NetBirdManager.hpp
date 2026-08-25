#pragma once

#include "Singleton.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct NetBirdPeer {
    std::string address;
    std::string name;
};

// Thin Artemis adapter around jmpangilinan/netbird-switch.
//
// Horizon has no TUN device, so the NetBird library owns a userspace
// lwIP/WireGuard stack and exposes a localhost TCP/UDP proxy. This manager
// keeps exactly one peer routed at a time, matching the library's model.
class NetBirdManager : public Singleton<NetBirdManager> {
public:
    NetBirdManager() = default;
    ~NetBirdManager();

    // Loads <Artemis working dir>/netbird.conf and connects on demand.
    bool ensure_connected();
    [[nodiscard]] bool ready() const;
    [[nodiscard]] std::string tunnel_ip() const;
    [[nodiscard]] std::string last_error() const;
    [[nodiscard]] std::string config_path() const;

    // NetBird Sync peers. The setup key is never returned or logged.
    std::vector<NetBirdPeer> peers();

    // If address belongs to a synced NetBird peer, prepare the TCP + UDP
    // proxy and return the equivalent localhost address. LAN/public addresses
    // pass through unchanged.
    std::string route_address(const std::string& address);

    void shutdown();

private:
    struct Config {
        std::string server;
        std::string setupKey;
    };

    bool load_config(Config& config, std::string& error) const;
    bool is_peer_address(const std::string& address) const;
    bool activate_peer(const std::string& address, unsigned short tcpPort);
    void start_poll_thread();

    mutable std::mutex stateMutex;
    mutable std::mutex routeMutex;
    std::atomic<bool> stopPolling{false};
    std::thread pollThread;
    bool initialized = false;
    std::string activePeer;
    std::string errorText;
};
