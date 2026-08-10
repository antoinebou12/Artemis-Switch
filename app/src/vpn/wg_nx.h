#pragma once

// Minimal C ABI expected from an optional wg-nx static library.
// Provide lib/switch/libwg_nx.a + include/wg_nx.h to enable a real tunnel.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WgNxTunnel WgNxTunnel;

WgNxTunnel* wg_nx_tunnel_create(const char* conf_text);
int wg_nx_tunnel_start(WgNxTunnel* tunnel);
void wg_nx_tunnel_stop(WgNxTunnel* tunnel);
void wg_nx_tunnel_destroy(WgNxTunnel* tunnel);
const char* wg_nx_tunnel_address(WgNxTunnel* tunnel);

#ifdef __cplusplus
}
#endif
