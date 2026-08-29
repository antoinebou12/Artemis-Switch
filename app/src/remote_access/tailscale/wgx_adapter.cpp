#include "wgx.h"

#include <wireguard.h>

#include <array>
#include <cstring>
#include <mutex>
#include <new>
#include <unordered_map>

extern "C" {
WgTunnel* tailscale_internal_wg_init(const WgConfig* config);
void tailscale_internal_wg_set_recv_callback(WgTunnel*, WgRecvCallback, void*);
int tailscale_internal_wg_start(WgTunnel*);
void tailscale_internal_wg_stop(WgTunnel*);
void tailscale_internal_wg_close(WgTunnel*);
WgPeer* tailscale_internal_wg_add_peer(
    WgTunnel*, const uint8_t[WG_KEY_LEN],
    const uint8_t[WG_RELAY_HASH_LEN], uint32_t);
WgPeer* tailscale_internal_wg_find_peer_by_ip(WgTunnel*, uint32_t);
int tailscale_internal_wg_update_peer(WgTunnel*, WgPeer*,
                                      const uint8_t[WG_KEY_LEN]);
int tailscale_internal_wg_remove_peer(WgTunnel*, WgPeer*);
int tailscale_internal_wg_connect_peer(WgTunnel*, WgPeer*);
int tailscale_internal_wg_send_to_peer(WgTunnel*, WgPeer*, const void*, size_t);
int tailscale_internal_wg_set_relay_send(
    WgTunnel*, void (*)(void*, const uint8_t*, const void*, size_t), void*);
int tailscale_internal_wg_relay_input(WgTunnel*, const void*, size_t);
}

struct WgxContext {
    WgTunnel* tunnel = nullptr;
    WgxEncryptedEgress encryptedEgress = nullptr;
    WgxPlaintextIngress plaintextIngress = nullptr;
    void* user = nullptr;
    std::mutex mutex;
    std::unordered_map<uint32_t, uint32_t> peerIps;
};

namespace {
uint32_t decodePeerId(const uint8_t* relayHash) {
    uint32_t id = 0;
    if (relayHash) std::memcpy(&id, relayHash, sizeof(id));
    return id;
}

void encryptedBridge(void* opaque, const uint8_t* relayHash, const void* packet,
                     size_t length) {
    auto* context = static_cast<WgxContext*>(opaque);
    if (context && context->encryptedEgress) {
        context->encryptedEgress(context->user, decodePeerId(relayHash),
                                 packet, length);
    }
}

int plaintextBridge(void* opaque, WgRecvSlot*, const void* packet,
                    size_t length) {
    auto* context = static_cast<WgxContext*>(opaque);
    if (context && context->plaintextIngress)
        context->plaintextIngress(context->user, packet, length);
    return 0;
}

WgPeer* findPeer(WgxContext* context, uint32_t peerId) {
    const auto found = context->peerIps.find(peerId);
    if (found == context->peerIps.end()) return nullptr;
    return tailscale_internal_wg_find_peer_by_ip(context->tunnel,
                                                 found->second);
}
} // namespace

extern "C" WgxContext* wgx_create(
    const uint8_t privateKey[32], uint32_t localIp,
    WgxEncryptedEgress encryptedEgress,
    WgxPlaintextIngress plaintextIngress, void* user) {
    if (!privateKey || !encryptedEgress || !plaintextIngress) return nullptr;
    auto* context = new (std::nothrow) WgxContext;
    if (!context) return nullptr;

    context->encryptedEgress = encryptedEgress;
    context->plaintextIngress = plaintextIngress;
    context->user = user;

    WgConfig config{};
    std::memcpy(config.private_key, privateKey, WG_KEY_LEN);
    config.tunnel_ip.s_addr = localIp;
    std::strcpy(config.endpoint_host, "127.0.0.1");
    config.endpoint_port = 1;
    config.keepalive_interval = 25;
    context->tunnel = tailscale_internal_wg_init(&config);
    if (!context->tunnel) {
        delete context;
        return nullptr;
    }
    if (tailscale_internal_wg_set_relay_send(
            context->tunnel, encryptedBridge, context) != 0) {
        tailscale_internal_wg_close(context->tunnel);
        delete context;
        return nullptr;
    }
    tailscale_internal_wg_set_recv_callback(context->tunnel, plaintextBridge,
                                            context);
    return context;
}

extern "C" int wgx_add_or_update_peer(
    WgxContext* context, uint32_t peerId, const uint8_t publicKey[32],
    uint32_t peerIp) {
    if (!context || !publicKey || peerId == 0 || peerIp == 0) return -1;
    std::lock_guard lock(context->mutex);
    if (auto* peer = findPeer(context, peerId)) {
        if (context->peerIps[peerId] != peerIp) return -1;
        return tailscale_internal_wg_update_peer(context->tunnel, peer,
                                                 publicKey);
    }
    std::array<uint8_t, WG_RELAY_HASH_LEN> transportId{};
    std::memcpy(transportId.data(), &peerId, sizeof(peerId));
    auto* peer = tailscale_internal_wg_add_peer(
        context->tunnel, publicKey, transportId.data(), peerIp);
    if (!peer) return -1;
    context->peerIps.emplace(peerId, peerIp);
    return 0;
}

extern "C" int wgx_remove_peer(WgxContext* context, uint32_t peerId) {
    if (!context) return -1;
    std::lock_guard lock(context->mutex);
    auto* peer = findPeer(context, peerId);
    if (!peer) return -1;
    const int result = tailscale_internal_wg_remove_peer(context->tunnel, peer);
    if (result == 0) context->peerIps.erase(peerId);
    return result;
}

extern "C" int wgx_start(WgxContext* context) {
    return context ? tailscale_internal_wg_start(context->tunnel) : -1;
}

extern "C" int wgx_connect_peer(WgxContext* context, uint32_t peerId) {
    if (!context) return -1;
    std::lock_guard lock(context->mutex);
    auto* peer = findPeer(context, peerId);
    return peer ? tailscale_internal_wg_connect_peer(context->tunnel, peer)
                : -1;
}

extern "C" int wgx_send_plaintext(WgxContext* context, uint32_t peerId,
                                   const void* packet, size_t length) {
    if (!context || !packet) return -1;
    std::lock_guard lock(context->mutex);
    auto* peer = findPeer(context, peerId);
    return peer ? tailscale_internal_wg_send_to_peer(context->tunnel, peer,
                                                     packet, length)
                : -1;
}

extern "C" int wgx_inject_encrypted(WgxContext* context, uint32_t peerId,
                                     const void* packet, size_t length) {
    if (!context || !packet || length == 0) return -1;
    {
        std::lock_guard lock(context->mutex);
        if (!findPeer(context, peerId)) return -1;
    }
    const int result = tailscale_internal_wg_relay_input(
        context->tunnel, packet, length);
    return result;
}

extern "C" void wgx_poll(WgxContext*) {}

extern "C" void wgx_destroy(WgxContext* context) {
    if (!context) return;
    tailscale_internal_wg_stop(context->tunnel);
    tailscale_internal_wg_close(context->tunnel);
    delete context;
}
