#include <cassert>
#include "../app/include/discovery_candidate.hpp"

int main() {
    DiscoveryCandidate direct;
    direct.targetAddress = "192.168.1.10";
    direct.providerId = "";
    assert(direct.isDirect());

    DiscoveryCandidate remote;
    remote.targetAddress = "100.64.0.5";
    remote.providerId = "netbird";
    remote.peerId = "peer123";
    remote.displayName = "Home";
    remote.priority = 10;
    assert(!remote.isDirect());
    assert(remote.providerId == "netbird");
    return 0;
}
