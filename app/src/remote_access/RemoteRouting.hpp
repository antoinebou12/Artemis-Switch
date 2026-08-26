#pragma once

#include "RemoteRouteLease.hpp"

#include <string>

namespace artemis::remote {

// The loopback address every proxied connection actually dials. The VPN library
// listens here and bridges to the peer through lwIP/WireGuard.
inline constexpr const char* kProxyAddress = "127.0.0.1";

// If `address` names a peer reachable through the active remote-access
// provider, start that peer's TCP/UDP proxies and return a live lease.
//
// The lease MUST outlive the whole session, not just the initial handshake:
// pairing, the app list, launch and the stream itself all travel through the
// same proxy, and releasing early tears it down mid-stream.
//
// Returns an inactive lease when the address is not a known peer, which is the
// normal case for LAN and direct-WAN addresses -- callers then connect to
// `address` unchanged.
RemoteRouteLease acquireRouteFor(const std::string& address);

// The address to hand to gs_init for `address`: loopback when `lease` is
// active, otherwise `address` untouched.
std::string connectAddressFor(const RemoteRouteLease& lease,
                              const std::string& address);

} // namespace artemis::remote
