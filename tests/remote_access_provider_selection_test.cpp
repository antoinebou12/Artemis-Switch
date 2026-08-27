#include <cassert>

#include "../app/include/remote_access_provider_id.hpp"

int main() {
    const RemoteAccessProviderId transitions[] = {
        RemoteAccessProviderId::Off,
        RemoteAccessProviderId::WireGuard,
        RemoteAccessProviderId::NetBird,
    };

    for (int selection = 0; selection < 3; ++selection) {
        assert(providerFromSelectorIndex(selection) == transitions[selection]);
    }
    assert(providerFromSelectorIndex(-1) == RemoteAccessProviderId::Off);
    assert(providerFromSelectorIndex(4) == RemoteAccessProviderId::Off);

    const auto off = providerVisibility(RemoteAccessProviderId::Off);
    assert(!off.wireGuard && !off.netBird);

    const auto wireGuard = providerVisibility(RemoteAccessProviderId::WireGuard);
    assert(wireGuard.wireGuard && !wireGuard.netBird);

    const auto netBird = providerVisibility(RemoteAccessProviderId::NetBird);
    assert(!netBird.wireGuard && netBird.netBird);

    assert(remoteAccessProviderRuntimeId(RemoteAccessProviderId::Off).empty());
    assert(remoteAccessProviderRuntimeId(RemoteAccessProviderId::WireGuard) ==
           "wireguard");
    assert(remoteAccessProviderRuntimeId(RemoteAccessProviderId::NetBird) ==
           "netbird");

    return 0;
}
