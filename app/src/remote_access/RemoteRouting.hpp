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

// Why the provider refused the route (its last error, or a generic reason).
// Only meaningful when lease.refused() is true.
std::string routeRefusalReason(const RemoteRouteLease& lease);

void logConnectionAttempt(const RemoteRouteLease& lease,
                          const std::string& requestedAddress,
                          const std::string& dialAddress);

void logConnectionResult(const RemoteRouteLease& lease,
                         const std::string& dialAddress, bool succeeded,
                         const std::string& detail = {});

// Records a pairing outcome in vpn.log when it ran through the loopback proxy
// (`dialAddress` == kProxyAddress); no-op for LAN/direct pairing.
void logProxiedPairingResult(const std::string& dialAddress, bool succeeded,
                             const std::string& detail = {});

// Records in vpn.log whether a host that was just paired/added ended up in the
// saved host list, so a silently dropped host is visible in the log.
void logHostSaveResult(const std::string& hostname, const std::string& address,
                       const std::string& mac, size_t hostsBefore,
                       size_t hostsAfter, bool foundAfterSave);

} // namespace artemis::remote
