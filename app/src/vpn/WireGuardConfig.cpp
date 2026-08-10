#include "WireGuardConfig.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {

std::string trim(std::string value) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(),
                value.end());
    return value;
}

} // namespace

std::string load_text_file(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

WireGuardConfig parse_wireguard_conf(const std::string& text) {
    WireGuardConfig config;
    enum class Section { None, Interface, Peer };
    Section section = Section::None;
    WireGuardPeerConfig peer;

    std::istringstream stream(text);
    std::string line;
    auto flushPeer = [&] {
        if (section == Section::Peer && !peer.publicKey.empty()) {
            config.peers.push_back(peer);
        }
        peer = {};
    };

    while (std::getline(stream, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        if (line == "[Interface]") {
            flushPeer();
            section = Section::Interface;
            continue;
        }
        if (line == "[Peer]") {
            flushPeer();
            section = Section::Peer;
            continue;
        }

        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));

        if (section == Section::Interface) {
            if (key == "PrivateKey") {
                config.privateKey = value;
            } else if (key == "Address") {
                config.address = value;
            } else if (key == "DNS") {
                config.dns = value;
            } else if (key == "ListenPort") {
                config.listenPort = std::atoi(value.c_str());
            }
        } else if (section == Section::Peer) {
            if (key == "PublicKey") {
                peer.publicKey = value;
            } else if (key == "Endpoint") {
                peer.endpoint = value;
            } else if (key == "AllowedIPs") {
                peer.allowedIps = value;
            } else if (key == "PersistentKeepalive") {
                peer.persistentKeepalive = std::atoi(value.c_str());
            }
        }
    }

    flushPeer();
    return config;
}
