#include "NetBirdManager.hpp"

#include "Settings.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <sstream>

#if defined(__SWITCH__)
#include "client.h"
extern "C" {
#include "netbird.h"
}
#endif

namespace {
std::string trim(std::string value) {
    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(),
                std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(),
                value.end());
    return value;
}

struct ParsedAddress {
    std::string host;
    std::string suffix;
    unsigned short port = 47989;
};

ParsedAddress parse_address(const std::string& address) {
    ParsedAddress parsed;
    parsed.host = address;

    // NetBird peers are IPv4 today. Only interpret a single colon as an
    // optional Moonlight TCP port; leave IPv6-like input untouched.
    const auto colon = address.find(':');
    if (colon == std::string::npos ||
        address.find(':', colon + 1) != std::string::npos) {
        return parsed;
    }

    const std::string portText = address.substr(colon + 1);
    if (portText.empty()) {
        return parsed;
    }

    try {
        const int port = std::stoi(portText);
        if (port > 0 && port <= 65535) {
            parsed.host = address.substr(0, colon);
            parsed.suffix = address.substr(colon);
            parsed.port = static_cast<unsigned short>(port);
        }
    } catch (...) {
        // Treat malformed input as a normal non-NetBird host address.
    }

    return parsed;
}
}

NetBirdManager::~NetBirdManager() { shutdown(); }

std::string NetBirdManager::config_path() const {
    const std::string workingDir = Settings::instance().working_dir();
    if (!workingDir.empty()) {
        const char last = workingDir.back();
        return workingDir + (last == '/' || last == '\\' ? "" : "/") +
               "netbird.conf";
    }
#if defined(__SWITCH__)
    return "sdmc:/switch/Artemis-Switch/netbird.conf";
#else
    return "netbird.conf";
#endif
}

bool NetBirdManager::load_config(Config& config, std::string& error) const {
    std::ifstream input(config_path());
    if (!input.is_open()) {
        error = "NetBird config not found: " + config_path();
        return false;
    }

    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }

        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));
        if (key == "server" || key == "management_server") {
            config.server = value;
        } else if (key == "setup_key") {
            config.setupKey = value;
        }
    }

    if (config.server.empty()) {
        error = "NetBird config is missing server=";
        return false;
    }
    if (config.setupKey.empty()) {
        error = "NetBird config is missing setup_key=";
        return false;
    }
    return true;
}

bool NetBirdManager::ensure_connected() {
#if !defined(__SWITCH__)
    return false;
#else
    if (netbird_is_ready()) {
        return true;
    }

    std::lock_guard<std::mutex> lock(stateMutex);
    if (netbird_is_ready()) {
        return true;
    }

    Config config;
    std::string configError;
    if (!load_config(config, configError)) {
        errorText = configError;
        return false;
    }

    char error[256]{};
    const int result = netbird_init(config.server.c_str(), config.setupKey.c_str(),
                                    error, sizeof(error));
    if (result != 0) {
        errorText = error[0] != '\0' ? error : "NetBird initialization failed";
        initialized = false;
        return false;
    }

    initialized = true;
    errorText.clear();
    start_poll_thread();
    return true;
#endif
}

void NetBirdManager::start_poll_thread() {
#if defined(__SWITCH__)
    if (pollThread.joinable()) {
        return;
    }

    stopPolling.store(false);
    pollThread = std::thread([this] {
        while (!stopPolling.load()) {
            netbird_poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    });
#endif
}

bool NetBirdManager::ready() const {
#if defined(__SWITCH__)
    return netbird_is_ready() != 0;
#else
    return false;
#endif
}

std::string NetBirdManager::tunnel_ip() const {
#if defined(__SWITCH__)
    const char* ip = netbird_get_ip();
    return ip ? ip : "";
#else
    return "";
#endif
}

std::string NetBirdManager::last_error() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return errorText;
}

std::vector<NetBirdPeer> NetBirdManager::peers() {
    std::vector<NetBirdPeer> result;
#if defined(__SWITCH__)
    if (!ensure_connected()) {
        return result;
    }

    const int count = netbird_get_peer_count();
    for (int index = 0; index < count; ++index) {
        char ip[64]{};
        char name[256]{};
        if (!netbird_get_peer(index, ip, sizeof(ip), name, sizeof(name))) {
            continue;
        }
        if (ip[0] == '\0') {
            continue;
        }
        result.push_back({ip, name[0] != '\0' ? name : ip});
    }
#endif
    return result;
}

bool NetBirdManager::is_peer_address(const std::string& address) const {
#if !defined(__SWITCH__)
    (void)address;
    return false;
#else
    const int count = netbird_get_peer_count();
    for (int index = 0; index < count; ++index) {
        char ip[64]{};
        char name[256]{};
        if (netbird_get_peer(index, ip, sizeof(ip), name, sizeof(name)) &&
            address == ip) {
            return true;
        }
    }
    return false;
#endif
}

bool NetBirdManager::activate_peer(const std::string& address,
                                   unsigned short tcpPort) {
#if !defined(__SWITCH__)
    (void)address;
    (void)tcpPort;
    return false;
#else
    std::lock_guard<std::mutex> lock(routeMutex);

    const char* current = netbird_proxy_target();
    if (!activePeer.empty() && activePeer == address && current != nullptr &&
        address == current) {
        return true;
    }

    netbird_proxy_stop_udp();
    netbird_proxy_stop();
    activePeer.clear();

    if (netbird_proxy_start(address.c_str(), tcpPort) != 0) {
        std::lock_guard<std::mutex> stateLock(stateMutex);
        errorText = "Failed to start NetBird TCP proxy for " + address;
        return false;
    }

    if (netbird_proxy_start_udp(address.c_str()) != 0) {
        netbird_proxy_stop();
        std::lock_guard<std::mutex> stateLock(stateMutex);
        errorText = "Failed to start NetBird UDP proxy for " + address;
        return false;
    }

    activePeer = address;
    return true;
#endif
}

std::string NetBirdManager::route_address(const std::string& address) {
#if !defined(__SWITCH__)
    return address;
#else
    const ParsedAddress parsed = parse_address(address);
    if (parsed.host.empty() || parsed.host == "127.0.0.1" ||
        parsed.host == "localhost") {
        return address;
    }

    // Never hijack arbitrary 100.x/LAN addresses. We only redirect an exact
    // peer returned by the authenticated NetBird Sync response.
    if (!ensure_connected() || !is_peer_address(parsed.host)) {
        return address;
    }

    if (!activate_peer(parsed.host, parsed.port)) {
        return address;
    }

    return std::string("127.0.0.1") + parsed.suffix;
#endif
}

void NetBirdManager::shutdown() {
#if defined(__SWITCH__)
    stopPolling.store(true);
    if (pollThread.joinable()) {
        pollThread.join();
    }

    {
        std::lock_guard<std::mutex> routeLock(routeMutex);
        if (initialized || netbird_is_ready()) {
            netbird_proxy_stop_udp();
            netbird_proxy_stop();
        }
        activePeer.clear();
    }

    std::lock_guard<std::mutex> stateLock(stateMutex);
    if (initialized || netbird_is_ready()) {
        netbird_shutdown();
    }
    initialized = false;
#endif
}

#if defined(__SWITCH__)
// GNU ld --wrap is used instead of spreading NetBird conditionals throughout
// GameStreamClient. LAN/public hosts pass through unchanged. A synced NetBird
// peer is redirected to the local proxy while SERVER_DATA continues to be
// populated by the unmodified gs_init implementation.
int artemis_real_gs_init(PSERVER_DATA server, const std::string address)
    asm("__real__Z7gs_initP12_SERVER_DATANSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE");
int artemis_wrapped_gs_init(PSERVER_DATA server, const std::string address)
    asm("__wrap__Z7gs_initP12_SERVER_DATANSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE");

int artemis_wrapped_gs_init(PSERVER_DATA server, const std::string address) {
    return artemis_real_gs_init(server,
                                NetBirdManager::instance().route_address(address));
}
#endif
