#pragma once
#include <string>

enum class RemoteAccessProviderId : int {
    Off = 0,
    WireGuard = 1,
    NetBird = 2,
    // Value 3 was used by an unreleased legacy selector mapping. Keep it
    // reserved so an old settings file can never enable a different backend.
    Tailscale = 4,
};

inline std::string toString(RemoteAccessProviderId id) {
    switch (id) {
        case RemoteAccessProviderId::Off:
            return "Off";
        case RemoteAccessProviderId::WireGuard:
            return "WireGuard";
        case RemoteAccessProviderId::NetBird:
            return "NetBird";
        case RemoteAccessProviderId::Tailscale:
            return "Tailscale";
    }
    return "Off";
}

inline RemoteAccessProviderId fromInt(int v) {
    switch (v) {
        case 0:
            return RemoteAccessProviderId::Off;
        case 1:
            return RemoteAccessProviderId::WireGuard;
        case 2:
            return RemoteAccessProviderId::NetBird;
        case 4:
            return RemoteAccessProviderId::Tailscale;
        default:
            return RemoteAccessProviderId::Off;
    }
}

inline RemoteAccessProviderId providerFromSelectorIndex(int selection) {
    switch (selection) {
        case 0:
            return RemoteAccessProviderId::Off;
        case 1:
            return RemoteAccessProviderId::WireGuard;
        case 2:
            return RemoteAccessProviderId::NetBird;
        case 3:
            return RemoteAccessProviderId::Tailscale;
        default:
            return RemoteAccessProviderId::Off;
    }
}

inline int selectorIndexFromProvider(RemoteAccessProviderId provider) {
    switch (provider) {
        case RemoteAccessProviderId::Off:
            return 0;
        case RemoteAccessProviderId::WireGuard:
            return 1;
        case RemoteAccessProviderId::NetBird:
            return 2;
        case RemoteAccessProviderId::Tailscale:
            return 3;
    }
    return 0;
}

inline std::string remoteAccessProviderRuntimeId(RemoteAccessProviderId id) {
    switch (id) {
        case RemoteAccessProviderId::WireGuard:
            return "wireguard";
        case RemoteAccessProviderId::NetBird:
            return "netbird";
        case RemoteAccessProviderId::Tailscale:
            return "tailscale";
        case RemoteAccessProviderId::Off:
            return {};
    }
    return {};
}

// Temporary builds used 0=WireGuard, 1=NetBird, 2=Tailscale, 3=Off.
// Keep that conversion separate from the stable selector/storage mapping.
inline RemoteAccessProviderId providerFromLegacyVpnValue(int value) {
    if (value == 0) return RemoteAccessProviderId::WireGuard;
    if (value == 1) return RemoteAccessProviderId::NetBird;
    return RemoteAccessProviderId::Off;
}

struct RemoteAccessProviderVisibility {
    bool wireGuard = false;
    bool netBird = false;
    bool tailscale = false;
};

inline RemoteAccessProviderVisibility providerVisibility(
    RemoteAccessProviderId provider) {
    return {
        provider == RemoteAccessProviderId::WireGuard,
        provider == RemoteAccessProviderId::NetBird,
        provider == RemoteAccessProviderId::Tailscale,
    };
}
