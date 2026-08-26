#pragma once

#include <string>
#include <string_view>

struct NetBirdStaticConfigResult {
    std::string wireguardConfig;
    std::string error;

    [[nodiscard]] bool valid() const { return error.empty(); }
};

// Converts the deliberately limited, offline NetBird JSON bundle into the
// standard WireGuard configuration consumed by the existing Switch backend.
// This does not implement NetBird management, signal, ICE, relay, or roaming.
NetBirdStaticConfigResult parse_netbird_static_config(std::string_view jsonText);
