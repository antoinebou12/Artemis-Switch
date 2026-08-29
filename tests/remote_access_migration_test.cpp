#include <cassert>
#include "../app/include/remote_access_provider_id.hpp"
#include "../app/src/utils/Settings.hpp"

int main() {
    // Test default is Off
    Settings s;
    assert(s.remote_access_provider() == RemoteAccessProviderId::Off);

    // Test migration mapping
    // Old 0 -> WireGuard
    RemoteAccessProviderId p0 = providerFromLegacyVpnValue(0);
    assert(p0 == RemoteAccessProviderId::WireGuard);
    // Old 1 -> NetBird
    RemoteAccessProviderId p1 = providerFromLegacyVpnValue(1);
    assert(p1 == RemoteAccessProviderId::NetBird);
    // Old temporary value 2 meant Tailscale. Stable Tailscale uses 4, so the
    // old ambiguous value remains disabled rather than being reinterpreted.
    RemoteAccessProviderId p2 = providerFromLegacyVpnValue(2);
    assert(p2 == RemoteAccessProviderId::Off);
    // Old 3 -> Off
    RemoteAccessProviderId p3 = providerFromLegacyVpnValue(3);
    assert(p3 == RemoteAccessProviderId::Off);

    // Test fromInt out of range defaults to Off
    assert(providerFromLegacyVpnValue(99) == RemoteAccessProviderId::Off);

    // Stable values use the selector/storage mapping without migration.
    assert(fromInt(0) == RemoteAccessProviderId::Off);
    assert(fromInt(1) == RemoteAccessProviderId::WireGuard);
    assert(fromInt(2) == RemoteAccessProviderId::NetBird);
    assert(fromInt(3) == RemoteAccessProviderId::Off);
    assert(fromInt(4) == RemoteAccessProviderId::Tailscale);

    return 0;
}
