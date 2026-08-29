#include <cassert>

#include "../app/include/remote_access_provider_id.hpp"

int main() {
    const RemoteAccessProviderId transitions[] = {
        RemoteAccessProviderId::Off,
        RemoteAccessProviderId::WireGuard,
        RemoteAccessProviderId::NetBird,
        RemoteAccessProviderId::Tailscale,
    };

    for (int selection = 0; selection < 4; ++selection) {
        assert(providerFromSelectorIndex(selection) == transitions[selection]);
    }
    assert(providerFromSelectorIndex(-1) == RemoteAccessProviderId::Off);
    assert(providerFromSelectorIndex(3) == RemoteAccessProviderId::Tailscale);
    assert(providerFromSelectorIndex(4) == RemoteAccessProviderId::Off);
    assert(selectorIndexFromProvider(RemoteAccessProviderId::Tailscale) == 3);

    const auto off = providerVisibility(RemoteAccessProviderId::Off);
    assert(!off.wireGuard && !off.netBird);

    const auto wireGuard = providerVisibility(RemoteAccessProviderId::WireGuard);
    assert(wireGuard.wireGuard && !wireGuard.netBird);

    const auto netBird = providerVisibility(RemoteAccessProviderId::NetBird);
    const auto tailscale = providerVisibility(RemoteAccessProviderId::Tailscale);
    assert(tailscale.tailscale && !tailscale.netBird && !tailscale.wireGuard);
    assert(!netBird.wireGuard && netBird.netBird);

    assert(remoteAccessProviderRuntimeId(RemoteAccessProviderId::Off).empty());
    assert(remoteAccessProviderRuntimeId(RemoteAccessProviderId::WireGuard) ==
           "wireguard");
    assert(remoteAccessProviderRuntimeId(RemoteAccessProviderId::NetBird) ==
           "netbird");
    assert(remoteAccessProviderRuntimeId(RemoteAccessProviderId::Tailscale) ==
           "tailscale");

    return 0;
}
