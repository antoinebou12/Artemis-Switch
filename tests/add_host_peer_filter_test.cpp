#include "../app/src/remote_access/AddHostPeerFilter.hpp"

#include <cassert>
#include <chrono>
#include <string>
#include <vector>

using namespace artemis::remote;

namespace {

RemoteAccessPeer peer(std::string id, std::string name, std::string os,
                      bool online = true) {
    RemoteAccessPeer p;
    p.providerId = "tailscale";
    p.peerId = std::move(id);
    p.name = std::move(name);
    p.address = "100.64.0.1";
    p.os = std::move(os);
    p.online = online;
    return p;
}

} // namespace

int main() {
    // --- classification -------------------------------------------------
    // Offline or address-less peers are never offered.
    assert(classifyPeerForAddHost(peer("a", "pc", "windows", false)) ==
           PeerVerdict::Hidden);
    {
        auto noAddress = peer("a", "pc", "windows");
        noAddress.address.clear();
        assert(classifyPeerForAddHost(noAddress) == PeerVerdict::Hidden);
    }
    // Phones and tablets are hidden without a probe.
    assert(classifyPeerForAddHost(peer("p", "phone", "android")) ==
           PeerVerdict::Hidden);
    assert(classifyPeerForAddHost(peer("p", "phone", "iOS")) ==
           PeerVerdict::Hidden);
    assert(isMobileOs("ipados") && !isMobileOs("windows") && !isMobileOs(""));
    // A reported GameStream port is enough to show the host.
    {
        auto reported = peer("pc", "pc", "windows");
        reported.tcpPortsKnown = true;
        reported.tcpPorts = {22, 47989};
        assert(classifyPeerForAddHost(reported) == PeerVerdict::Host);
        reported.tcpPorts = {47984};
        assert(classifyPeerForAddHost(reported) == PeerVerdict::Host);
        // A real port list without GameStream still gets probed: the list
        // may be partial.
        reported.tcpPorts = {22, 80};
        assert(classifyPeerForAddHost(reported) == PeerVerdict::Probe);
    }
    // Nothing reported (the common case): probe desktops and servers.
    assert(classifyPeerForAddHost(peer("pc", "pc", "windows")) ==
           PeerVerdict::Probe);
    assert(classifyPeerForAddHost(peer("srv", "srv", "linux")) ==
           PeerVerdict::Probe);
    assert(classifyPeerForAddHost(peer("x", "x", "")) == PeerVerdict::Probe);

    // --- display name ---------------------------------------------------
    assert(peerDisplayName(peer("pc", "desktop-c9uerv5.tail99a739.ts.net.",
                                "windows")) ==
           "desktop-c9uerv5.tail99a739.ts.net");
    assert(peerDisplayName(peer("pc", "", "windows")) == "100.64.0.1");

    // --- subnet shortcuts -----------------------------------------------
    assert(subnetInputPrefix("192.168.1.0/24") == "192.168.1.");
    assert(subnetInputPrefix("10.20.0.0/16") == "10.20.");
    assert(subnetInputPrefix("10.0.0.0/8") == "10.");
    assert(subnetInputPrefix("172.16.0.0/20") == "172.16.");
    assert(subnetInputPrefix("10.0.0.5/32") == "10.0.0.");
    assert(subnetInputPrefix("0.0.0.0/0").empty());
    assert(subnetInputPrefix("10.0.0.0/7").empty());
    assert(subnetInputPrefix("192.168.1.0").empty());
    assert(subnetInputPrefix("192.168.1.0/x").empty());
    {
        auto router = peer("r", "openwrt-main.tail.ts.net.", "linux");
        router.subnets = {"192.168.1.0/24", "10.0.0.0/8"};
        auto backup = peer("b", "backup-router", "linux");
        backup.subnets = {"192.168.1.0/24"}; // same LAN, listed once
        auto offline = peer("o", "old-router", "linux", false);
        offline.subnets = {"172.16.0.0/16"};
        const auto shortcuts = subnetShortcuts({router, backup, offline});
        assert(shortcuts.size() == 2);
        assert(shortcuts[0].subnet == "192.168.1.0/24");
        assert(shortcuts[0].inputPrefix == "192.168.1.");
        assert(shortcuts[0].viaName == "openwrt-main.tail.ts.net");
        assert(shortcuts[1].subnet == "10.0.0.0/8" &&
               shortcuts[1].inputPrefix == "10.");
    }

    // --- probe cache ----------------------------------------------------
    {
        ProbeCache cache;
        const auto t0 = ProbeCache::Clock::time_point{} + std::chrono::hours(1);
        assert(!cache.lookup("pc", t0));
        cache.store("pc", true, t0);
        cache.store("srv", false, t0);
        assert(cache.lookup("pc", t0) == true);
        assert(cache.lookup("srv", t0) == false);
        // A "no host" answer expires so a woken PC appears again; a found
        // host stays known.
        const auto later = t0 + ProbeCache::kNegativeTtl + std::chrono::seconds(1);
        assert(!cache.lookup("srv", later));
        assert(cache.lookup("pc", later) == true);
        // Refresh forgets only the misses.
        cache.store("srv", false, t0);
        cache.forgetMisses();
        assert(!cache.lookup("srv", t0));
        assert(cache.lookup("pc", t0) == true);
        cache.clear();
        assert(!cache.lookup("pc", t0));
    }
    return 0;
}
