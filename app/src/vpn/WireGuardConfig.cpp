#include "WireGuardConfig.hpp"

#include <algorithm>
#include <cctype>
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

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

// std::atoi cannot distinguish "0" from garbage, which silently turned a typo
// in ListenPort or PersistentKeepalive into 0. Return false instead.
bool parse_int(const std::string& value, int& out) {
    if (value.empty())
        return false;
    std::size_t consumed = 0;
    long parsed = 0;
    try {
        parsed = std::stol(value, &consumed);
    } catch (...) {
        return false;
    }
    if (consumed != value.size())
        return false;
    if (parsed < 0 || parsed > 0x7fffffff)
        return false;
    out = static_cast<int>(parsed);
    return true;
}

// WireGuard keys are 32 raw bytes in standard base64: 43 payload chars plus one
// '=' of padding.
bool looks_like_wg_key(const std::string& value) {
    if (value.size() != 44)
        return false;
    if (value.back() != '=')
        return false;
    for (std::size_t i = 0; i < 43; ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        const bool ok = std::isalnum(c) || c == '+' || c == '/';
        if (!ok)
            return false;
    }
    return true;
}

bool valid_ipv4(const std::string& value) {
    int octets = 0;
    std::size_t pos = 0;
    while (pos <= value.size()) {
        const auto dot = value.find('.', pos);
        const auto part = value.substr(pos, dot == std::string::npos
                                                ? std::string::npos
                                                : dot - pos);
        if (part.empty() || part.size() > 3)
            return false;
        for (char c : part) {
            if (!std::isdigit(static_cast<unsigned char>(c)))
                return false;
        }
        int number = 0;
        if (!parse_int(part, number) || number > 255)
            return false;
        ++octets;
        if (dot == std::string::npos)
            break;
        pos = dot + 1;
    }
    return octets == 4;
}

// Accepts "10.0.0.2", "10.0.0.2/32", and IPv6 forms. IPv6 is only checked
// loosely -- enough to reject obvious typos without reimplementing inet_pton.
bool valid_address(const std::string& value) {
    if (value.empty())
        return false;
    // A conf may list several comma-separated addresses; all must parse.
    std::size_t pos = 0;
    while (pos <= value.size()) {
        const auto comma = value.find(',', pos);
        auto entry = trim(value.substr(
            pos, comma == std::string::npos ? std::string::npos : comma - pos));
        if (entry.empty())
            return false;

        const auto slash = entry.find('/');
        if (slash != std::string::npos) {
            int prefix = 0;
            if (!parse_int(entry.substr(slash + 1), prefix))
                return false;
            entry = entry.substr(0, slash);
            const bool ipv6 = entry.find(':') != std::string::npos;
            if (prefix > (ipv6 ? 128 : 32))
                return false;
        }

        if (entry.find(':') != std::string::npos) {
            // IPv6: hex digits and colons only.
            for (char c : entry) {
                if (!std::isxdigit(static_cast<unsigned char>(c)) && c != ':')
                    return false;
            }
        } else if (!valid_ipv4(entry)) {
            return false;
        }

        if (comma == std::string::npos)
            break;
        pos = comma + 1;
    }
    return true;
}

// "host:port" or "[v6]:port". The host half is left permissive because a
// hostname is legal here; the port is what people actually get wrong.
bool valid_endpoint(const std::string& value) {
    if (value.empty())
        return false;
    const auto colon = value.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= value.size())
        return false;
    int port = 0;
    if (!parse_int(value.substr(colon + 1), port))
        return false;
    return port >= 1 && port <= 65535;
}

} // namespace

void wireguard_scrub(std::string& secret) {
    for (auto& c : secret) {
        c = '\0';
    }
    secret.clear();
}

const char* wireguard_problem_i18n_key(WireGuardConfig::Problem problem) {
    switch (problem) {
        case WireGuardConfig::Problem::None:
            return "artemis/settings/wireguard_problem_none";
        case WireGuardConfig::Problem::MissingPrivateKey:
            return "artemis/settings/wireguard_problem_missing_private_key";
        case WireGuardConfig::Problem::MalformedPrivateKey:
            return "artemis/settings/wireguard_problem_malformed_private_key";
        case WireGuardConfig::Problem::MissingAddress:
            return "artemis/settings/wireguard_problem_missing_address";
        case WireGuardConfig::Problem::MalformedAddress:
            return "artemis/settings/wireguard_problem_malformed_address";
        case WireGuardConfig::Problem::NoPeers:
            return "artemis/settings/wireguard_problem_no_peers";
        case WireGuardConfig::Problem::MissingPeerPublicKey:
            return "artemis/settings/wireguard_problem_missing_peer_key";
        case WireGuardConfig::Problem::MalformedPeerPublicKey:
            return "artemis/settings/wireguard_problem_malformed_peer_key";
        case WireGuardConfig::Problem::MalformedPeerPresharedKey:
            return "artemis/settings/wireguard_problem_malformed_psk";
        case WireGuardConfig::Problem::MalformedEndpoint:
            return "artemis/settings/wireguard_problem_malformed_endpoint";
        case WireGuardConfig::Problem::InvalidListenPort:
            return "artemis/settings/wireguard_problem_listen_port";
        case WireGuardConfig::Problem::InvalidKeepalive:
            return "artemis/settings/wireguard_problem_keepalive";
        case WireGuardConfig::Problem::InvalidMtu:
            return "artemis/settings/wireguard_problem_mtu";
    }
    return "artemis/settings/wireguard_problem_none";
}

WireGuardConfig::Problem WireGuardConfig::validate() const {
    if (privateKey.empty())
        return Problem::MissingPrivateKey;
    if (!looks_like_wg_key(privateKey))
        return Problem::MalformedPrivateKey;
    if (address.empty())
        return Problem::MissingAddress;
    if (!valid_address(address))
        return Problem::MalformedAddress;
    // 0 means "unset"; the parser rejects non-numeric input separately.
    if (listenPort != 0 && (listenPort < 1 || listenPort > 65535))
        return Problem::InvalidListenPort;
    if (mtu != 0 && (mtu < 576 || mtu > 1500))
        return Problem::InvalidMtu;
    if (peers.empty())
        return Problem::NoPeers;

    for (const auto& peer : peers) {
        if (peer.publicKey.empty())
            return Problem::MissingPeerPublicKey;
        if (!looks_like_wg_key(peer.publicKey))
            return Problem::MalformedPeerPublicKey;
        if (!peer.presharedKey.empty() && !looks_like_wg_key(peer.presharedKey))
            return Problem::MalformedPeerPresharedKey;
        if (!peer.endpoint.empty() && !valid_endpoint(peer.endpoint))
            return Problem::MalformedEndpoint;
        if (peer.persistentKeepalive < 0 || peer.persistentKeepalive > 65535)
            return Problem::InvalidKeepalive;
    }
    return Problem::None;
}

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
    // A malformed number must fail validation rather than silently reading as
    // 0, so remember that we saw one.
    bool badListenPort = false;
    bool badMtu = false;
    bool badKeepalive = false;

    std::istringstream stream(text);
    std::string line;
    auto flushPeer = [&] {
        if (section == Section::Peer && !peer.publicKey.empty()) {
            config.peers.push_back(peer);
        }
        peer = {};
    };

    while (std::getline(stream, line)) {
        // wg-quick treats both '#' and ';' as comment introducers.
        const auto comment = line.find_first_of("#;");
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        // Section headers are matched case-insensitively and after trimming,
        // so "[peer]" and "[Peer]  " both work like they do in wg-quick.
        if (line.front() == '[') {
            const auto header = lowercase(line);
            if (header == "[interface]") {
                flushPeer();
                section = Section::Interface;
                continue;
            }
            if (header == "[peer]") {
                flushPeer();
                section = Section::Peer;
                continue;
            }
            // Unknown section: stop attributing keys to the previous one.
            flushPeer();
            section = Section::None;
            continue;
        }

        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = lowercase(trim(line.substr(0, eq)));
        const std::string value = trim(line.substr(eq + 1));

        if (section == Section::Interface) {
            if (key == "privatekey") {
                config.privateKey = value;
            } else if (key == "address") {
                config.address = value;
            } else if (key == "dns") {
                config.dns = value;
            } else if (key == "listenport") {
                if (!parse_int(value, config.listenPort))
                    badListenPort = true;
            } else if (key == "mtu") {
                if (!parse_int(value, config.mtu))
                    badMtu = true;
            }
        } else if (section == Section::Peer) {
            if (key == "publickey") {
                peer.publicKey = value;
            } else if (key == "presharedkey") {
                peer.presharedKey = value;
            } else if (key == "endpoint") {
                peer.endpoint = value;
            } else if (key == "allowedips") {
                peer.allowedIps = value;
            } else if (key == "persistentkeepalive") {
                if (!parse_int(value, peer.persistentKeepalive))
                    badKeepalive = true;
            }
        }
    }

    flushPeer();

    // Force the matching validate() failure for unparseable numbers.
    if (badListenPort)
        config.listenPort = -1;
    if (badMtu)
        config.mtu = -1;
    if (badKeepalive && !config.peers.empty())
        config.peers.front().persistentKeepalive = -1;

    return config;
}
