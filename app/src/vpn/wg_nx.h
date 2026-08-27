#pragma once

// Minimal C ABI expected from an optional wg-nx static library.
// Provide lib/switch/libwg_nx.a + include/wg_nx.h to enable a real tunnel.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WgNxTunnel WgNxTunnel;

// False for the built-in stub, which validates a config but moves no packets.
// WireGuardManager uses this so the UI never claims a tunnel is "Running" when
// nothing is actually being tunnelled. A real libwg_nx.a returns true.
int wg_nx_is_real_backend(void);

WgNxTunnel* wg_nx_tunnel_create(const char* conf_text);
int wg_nx_tunnel_start(WgNxTunnel* tunnel);
int wg_nx_tunnel_can_route(WgNxTunnel* tunnel, const char* ipv4_address);
int wg_nx_tunnel_activate_route(WgNxTunnel* tunnel, const char* ipv4_address);
int wg_nx_tunnel_prepare_stream(WgNxTunnel* tunnel, const char* ipv4_address);
void wg_nx_tunnel_deactivate_route(WgNxTunnel* tunnel,
                                   const char* ipv4_address);
void wg_nx_tunnel_stop(WgNxTunnel* tunnel);
void wg_nx_tunnel_destroy(WgNxTunnel* tunnel);
const char* wg_nx_tunnel_address(WgNxTunnel* tunnel);

#ifdef __cplusplus
}
#endif
