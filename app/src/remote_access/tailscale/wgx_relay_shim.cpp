// Built INTO libtailscale-wgx.a by cmake/TailscaleWgxBackend.cmake. It is NOT
// part of the app's own sources.
//
// The loopback relay drives WireGuard and lwIP. Compiled into the app, those
// calls bound to the standalone WireGuard provider's un-namespaced
// libwireguard.a instead of the Tailscale copy, so Tailscale traffic never
// reached the DERP transport. Compiled inside the archive, every symbol here
// and in relay/TsLwipRelay.cpp is renamed to tailscale_internal_*, so the
// relay runs entirely on the Tailscale copy. The app reaches it only through
// these C entry points, wrapped by wgx_adapter.cpp.

#include "TsLwipRelay.hpp"

#include <cstdint>
#include <new>

namespace {
using RelayLog = void (*)(int level, const char* message);
}

extern "C" {

void* wgx_lwip_relay_new(WgTunnel* tunnel, RelayLog log) {
    if (!tunnel)
        return nullptr;
    artemis_tsrelay::LogFn sink;
    if (log)
        sink = [log](int level, const char* message) { log(level, message); };
    return new (std::nothrow) artemis_tsrelay::TsLwipRelay(tunnel, sink);
}

int wgx_lwip_relay_begin(void* relay, const char* tunnelIp,
                         const char* targetIp) {
    if (!relay || !tunnelIp || !targetIp)
        return -1;
    return static_cast<artemis_tsrelay::TsLwipRelay*>(relay)->start(tunnelIp,
                                                                   targetIp)
               ? 0
               : -1;
}

int wgx_lwip_relay_tcp(void* relay, std::uint16_t port) {
    return relay && static_cast<artemis_tsrelay::TsLwipRelay*>(relay)->addTcp(port)
               ? 0
               : -1;
}

int wgx_lwip_relay_udp(void* relay, std::uint16_t port) {
    return relay && static_cast<artemis_tsrelay::TsLwipRelay*>(relay)->addUdp(port)
               ? 0
               : -1;
}

int wgx_lwip_relay_running(void* relay) {
    return relay && static_cast<artemis_tsrelay::TsLwipRelay*>(relay)->isRunning()
               ? 1
               : 0;
}

void wgx_lwip_relay_delete(void* relay) {
    delete static_cast<artemis_tsrelay::TsLwipRelay*>(relay);
}

} // extern "C"
