#pragma once
#include <string>

enum class RemoteAccessProviderId : int {
    Off = 0,
    WireGuard = 1,
    NetBird = 2,
};

inline std::string toString(RemoteAccessProviderId id) {
    switch (id) {
        case RemoteAccessProviderId::Off: return "Off";
        case RemoteAccessProviderId::WireGuard: return "WireGuard";
        case RemoteAccessProviderId::NetBird: return "NetBird";
    }
    return "Off";
}

inline RemoteAccessProviderId fromInt(int v) {
    switch (v) {
        case 0: return RemoteAccessProviderId::Off;
        case 1: return RemoteAccessProviderId::WireGuard;
        case 2: return RemoteAccessProviderId::NetBird;
        default: return RemoteAccessProviderId::Off;
    }
}

inline RemoteAccessProviderId providerFromSelectorIndex(int selection) {
    return fromInt(selection);
}

inline std::string remoteAccessProviderRuntimeId(RemoteAccessProviderId id) {
    switch (id) {
        case RemoteAccessProviderId::WireGuard: return "wireguard";
        case RemoteAccessProviderId::NetBird: return "netbird";
        case RemoteAccessProviderId::Off: return {};
    }
    return {};
}

// Temporary builds used 0=WireGuard, 1=NetBird, 2=Tailscale, 3=Off.
// Keep that conversion separate from the stable selector/storage mapping.
inline RemoteAccessProviderId providerFromLegacyVpnValue(int value) {
    switch (value) {
        case 0: return RemoteAccessProviderId::WireGuard;
        case 1: return RemoteAccessProviderId::NetBird;
        case 2: return RemoteAccessProviderId::Off;
        case 3: return RemoteAccessProviderId::Off;
        default: return RemoteAccessProviderId::Off;
    }
}

struct RemoteAccessProviderVisibility {
    bool wireGuard = false;
    bool netBird = false;
};

inline RemoteAccessProviderVisibility providerVisibility(
    RemoteAccessProviderId provider) {
    return {
        provider == RemoteAccessProviderId::WireGuard,
        provider == RemoteAccessProviderId::NetBird,
    };
}
