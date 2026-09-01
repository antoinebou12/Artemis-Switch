#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace artemis::host {

// Manual overrides on top of the paired host's stored MAC. A host that reports
// an all-zero MAC at pair time (Sunshine can do this when it cannot resolve its
// own interface) can still be woken with an explicit MAC; a WAN/DDNS target can
// be named instead of relying on the LAN fan-out.
struct WakeOnLanOverride {
    std::string mac;                 // explicit MAC, e.g. "AA:BB:CC:DD:EE:FF"
    std::string address;             // explicit wake target (address[:port])
    unsigned short port = 0;         // 0 = default multiport fan-out
    std::string secureOnPassword;    // 6-byte SecureOn password (12 hex chars)
    int resendAttempts = 0;          // how many times a caller resends
};

// Normalizes a MAC (uppercase hex, separators removed, zero-MAC rejected).
// Returns 12 hex chars or an empty string when unusable.
std::string normalizeMacAddress(const std::string& mac);

// True when a manual MAC is usable even if the stored host MAC is not.
bool hasUsableOverrideMac(const WakeOnLanOverride& over);

// Expands a mac (stored or override-influenced) into an explicit target list.
// Override address is always first when present; then the host connection
// addresses plus the override MAC when it differs from the stored one.
std::vector<std::string> wakeTargetAddresses(
    const std::string& storedMac, const WakeOnLanOverride& over,
    const std::vector<std::string>& hostAddresses, bool includeBroadcast);

// Ports to fan out to. When the override names an explicit port, only that
// port is used; otherwise the static + dynamic sunshine-relative ports.
std::vector<unsigned short> wakeTargetPorts(const WakeOnLanOverride& over,
                                            unsigned short basePort);

// Builds the 102-byte Wake-on-LAN magic packet, or 108 bytes when a SecureOn
// password is present. Returns empty on an unusable MAC or password.
std::vector<std::uint8_t> buildMagicPacket(
    const std::string& normalizedMac, const std::string& secureOnPassword);

} // namespace artemis::host