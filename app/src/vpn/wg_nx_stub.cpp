#include "wg_nx.h"

#include "WireGuardConfig.hpp"

// A real backend should take SocketFdLock::instance().guard() (see
// SocketFdLock.hpp) around its handshake/UDP setup and teardown; the stub has
// no sockets to protect, so it does not include it.

// Stub backend so Switch builds succeed without vendoring wg-nx.
//
// This performs NO key exchange, opens NO socket, and tunnels NO traffic. It
// only parses and validates the config so the settings UI can give real
// feedback on the file. wg_nx_is_real_backend() returns false, and
// WireGuardManager uses that to report "unavailable" rather than "running" --
// an earlier version of this stub reported success and the UI happily claimed
// a working tunnel.
//
// Replace by linking a real libwg_nx.a exporting the same symbols.

struct WgNxTunnel {
    WireGuardConfig config;
};

extern "C" int wg_nx_is_real_backend(void) { return 0; }

extern "C" WgNxTunnel* wg_nx_tunnel_create(const char* conf_text) {
    if (!conf_text) {
        return nullptr;
    }
    auto* tunnel = new WgNxTunnel();
    tunnel->config = parse_wireguard_conf(conf_text);
    if (!tunnel->config.valid()) {
        wireguard_scrub(tunnel->config.privateKey);
        delete tunnel;
        return nullptr;
    }
    return tunnel;
}

extern "C" int wg_nx_tunnel_start(WgNxTunnel* tunnel) {
    if (!tunnel) {
        return -1;
    }
    // A real wg-nx build performs the handshake and UDP setup here, under the
    // same SocketFdLock. There is nothing to start in the stub, and reporting
    // success would be a lie, so refuse.
    return -1;
}

extern "C" int wg_nx_tunnel_can_route(WgNxTunnel* tunnel,
                                         const char* ipv4_address) {
    (void)tunnel;
    (void)ipv4_address;
    return 0;
}

extern "C" int wg_nx_tunnel_activate_route(WgNxTunnel* tunnel,
                                              const char* ipv4_address) {
    (void)tunnel;
    (void)ipv4_address;
    return -1;
}

extern "C" int wg_nx_tunnel_prepare_stream(WgNxTunnel* tunnel,
                                              const char* ipv4_address) {
    (void)tunnel;
    (void)ipv4_address;
    return -1;
}

extern "C" void wg_nx_tunnel_deactivate_route(WgNxTunnel* tunnel,
                                                 const char* ipv4_address) {
    (void)tunnel;
    (void)ipv4_address;
}

extern "C" void wg_nx_tunnel_stop(WgNxTunnel* tunnel) {
    (void)tunnel;
}

extern "C" void wg_nx_tunnel_destroy(WgNxTunnel* tunnel) {
    if (!tunnel) {
        return;
    }
    wg_nx_tunnel_stop(tunnel);
    // Do not leave the private key sitting in freed heap.
    wireguard_scrub(tunnel->config.privateKey);
    for (auto& peer : tunnel->config.peers) {
        wireguard_scrub(peer.presharedKey);
    }
    delete tunnel;
}

extern "C" const char* wg_nx_tunnel_address(WgNxTunnel* tunnel) {
    if (!tunnel) {
        return "";
    }
    return tunnel->config.address.c_str();
}
