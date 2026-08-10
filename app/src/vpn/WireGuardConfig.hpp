#pragma once

#include <mutex>
#include <string>
#include <vector>

struct WireGuardPeerConfig {
    std::string publicKey;
    std::string endpoint;
    std::string allowedIps;
    int persistentKeepalive = 0;
};

struct WireGuardConfig {
    std::string privateKey;
    std::string address;
    std::string dns;
    int listenPort = 0;
    std::vector<WireGuardPeerConfig> peers;

    [[nodiscard]] bool valid() const {
        return !privateKey.empty() && !address.empty() && !peers.empty() &&
               !peers.front().publicKey.empty();
    }
};

// Parse a standard WireGuard .conf (Interface + Peer sections).
WireGuardConfig parse_wireguard_conf(const std::string& text);
std::string load_text_file(const std::string& path);
