#include "wg_nx.h"

#include "SocketFdLock.hpp"
#include "WireGuardConfig.hpp"

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>

// Weak stub backend so Switch builds succeed without vendoring wg-nx yet.
// Replace by linking a real libwg_nx.a that exports the same symbols.

struct WgNxTunnel {
    WireGuardConfig config;
    bool running = false;
};

extern "C" WgNxTunnel* wg_nx_tunnel_create(const char* conf_text) {
    if (!conf_text) {
        return nullptr;
    }
    auto* tunnel = new WgNxTunnel();
    tunnel->config = parse_wireguard_conf(conf_text);
    if (!tunnel->config.valid()) {
        delete tunnel;
        return nullptr;
    }
    return tunnel;
}

extern "C" int wg_nx_tunnel_start(WgNxTunnel* tunnel) {
    if (!tunnel) {
        return -1;
    }
    auto lock = SocketFdLock::instance().guard();
    // Stub: mark running after validating config. A real wg-nx build performs
    // handshake/UDP here under the same SocketFdLock.
    tunnel->running = true;
    return 0;
}

extern "C" void wg_nx_tunnel_stop(WgNxTunnel* tunnel) {
    if (!tunnel) {
        return;
    }
    auto lock = SocketFdLock::instance().guard();
    tunnel->running = false;
}

extern "C" void wg_nx_tunnel_destroy(WgNxTunnel* tunnel) {
    if (!tunnel) {
        return;
    }
    wg_nx_tunnel_stop(tunnel);
    delete tunnel;
}

extern "C" const char* wg_nx_tunnel_address(WgNxTunnel* tunnel) {
    if (!tunnel) {
        return "";
    }
    return tunnel->config.address.c_str();
}
