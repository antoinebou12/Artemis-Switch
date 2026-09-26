#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WgxContext WgxContext;

typedef void (*WgxEncryptedEgress)(void* user, uint32_t peer_id,
                                   const void* packet, size_t length);
typedef void (*WgxPlaintextIngress)(void* user, const void* packet,
                                   size_t length);

WgxContext* wgx_create(const uint8_t private_key[32], uint32_t local_ip,
                       WgxEncryptedEgress encrypted_egress,
                       WgxPlaintextIngress plaintext_ingress, void* user);
int wgx_add_or_update_peer(WgxContext* context, uint32_t peer_id,
                           const uint8_t public_key[32], uint32_t peer_ip);
int wgx_remove_peer(WgxContext* context, uint32_t peer_id);
int wgx_start(WgxContext* context);
int wgx_connect_peer(WgxContext* context, uint32_t peer_id);
int wgx_send_plaintext(WgxContext* context, uint32_t peer_id,
                       const void* packet, size_t length);
int wgx_inject_encrypted(WgxContext* context, uint32_t peer_id,
                         const void* packet, size_t length);
void wgx_poll(WgxContext* context);
void wgx_destroy(WgxContext* context);

// Loopback TCP/UDP proxy (lwIP) bound to this context's tunnel. It runs on the
// Tailscale copy of wg-nx/lwIP, never on the standalone WireGuard provider's.
// Destroy the relay before the context.
typedef struct WgxRelay WgxRelay;
typedef void (*WgxRelayLog)(int level, const char* message);

WgxRelay* wgx_relay_create(WgxContext* context, WgxRelayLog log);
int wgx_relay_start(WgxRelay* relay, const char* local_ip, const char* peer_ip);
int wgx_relay_add_tcp(WgxRelay* relay, uint16_t port);
int wgx_relay_add_udp(WgxRelay* relay, uint16_t port);
int wgx_relay_is_running(WgxRelay* relay);
void wgx_relay_destroy(WgxRelay* relay);

#ifdef __cplusplus
}
#endif
