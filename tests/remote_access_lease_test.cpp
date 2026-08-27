#include <cassert>
#include "../app/src/remote_access/RemoteAccessManager.hpp"

int main() {
    auto& mgr = RemoteAccessManager::instance();
    // Test reference counting via activate/deactivate
    // No providers registered in test, so activation should fail gracefully
    bool ok = mgr.activateRoute("nonexistent", "peer1");
    assert(!ok);
    // Deactivate should be no-op
    mgr.deactivateRoute("nonexistent", "peer1");
    return 0;
}
