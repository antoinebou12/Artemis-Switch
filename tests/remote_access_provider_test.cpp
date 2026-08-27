#include "../app/src/remote_access/RemoteAccessManager.hpp"
#include "../app/src/remote_access/providers/WireGuardProvider.hpp"
#include "../app/src/remote_access/providers/NetBirdProvider.hpp"
#include <cassert>

int main() {
    auto& mgr = RemoteAccessManager::instance();
    mgr.registerProvider(std::make_unique<WireGuardProvider>());
    mgr.registerProvider(std::make_unique<NetBirdProvider>());

    auto providers = mgr.providers();
    assert(providers.size() >= 2);
    bool hasWireGuard = false;
    bool hasNetBird = false;
    for (auto* p : providers) {
        if (p->id() == "wireguard") hasWireGuard = true;
        if (p->id() == "netbird") hasNetBird = true;
    }
    assert(hasWireGuard);
    assert(hasNetBird);
    return 0;
}
