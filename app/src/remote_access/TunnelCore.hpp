#pragma once
#include <string>

struct WireGuardPeer {
    std::string publicKey;
    std::string endpoint;
    std::string allowedIps;
    int keepalive = 25;
};

class TunnelCore {
public:
    static TunnelCore& instance();
    bool init();
    bool start();
    void poll();
    void stop();
    bool addPeer(const WireGuardPeer& peer);
    bool setInterface(const std::string& privateKey, const std::string& address);
private:
    TunnelCore() = default;
    bool initialized_ = false;
};
