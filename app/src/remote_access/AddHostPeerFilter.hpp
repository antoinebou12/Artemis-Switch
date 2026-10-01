#pragma once

// Decides which remote-access peers the Add Host screen offers. Pure logic
// (no sockets, no UI), so it is host-tested; the probe itself lives in
// add_host_tab.cpp.

#include "IRemoteAccessProvider.hpp"

#include <chrono>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace artemis::remote {

enum class PeerVerdict {
    Hidden, // offline, no address, or a phone/tablet: never a GameStream host
    Host,   // the peer reports a GameStream port: show it without probing
    Probe,  // unknown: ask its GameStream server before showing it
};

// GameStream HTTP and HTTPS ports used by Sunshine, Apollo and Vibepollo.
inline constexpr std::uint16_t kGameStreamHttpPort = 47989;
inline constexpr std::uint16_t kGameStreamHttpsPort = 47984;

// Phones and tablets cannot run Sunshine/Apollo/Vibepollo.
[[nodiscard]] bool isMobileOs(std::string_view os);

[[nodiscard]] PeerVerdict classifyPeerForAddHost(const RemoteAccessPeer& peer);

// Peer name as shown in the list: control sends the fully qualified name with
// a trailing dot ("desktop.tailnet.ts.net."); the dot is dropped.
[[nodiscard]] std::string peerDisplayName(const RemoteAccessPeer& peer);

// "Add host on 192.168.1.0/24" rows: one per LAN subnet a peer shares.
struct SubnetShortcut {
    std::string subnet;      // "192.168.1.0/24"
    std::string inputPrefix; // "192.168.1." pre-typed in the IP keyboard
    std::string viaName;     // the router's display name
};

// Online peers only, deduplicated by subnet, in the order peers are listed.
[[nodiscard]] std::vector<SubnetShortcut>
subnetShortcuts(const std::vector<RemoteAccessPeer>& peers);

// The whole octets of a subnet's network address, for pre-typing:
// /24 -> "a.b.c.", /16 -> "a.b.", /8 -> "a.". Prefixes shorter than /8 give
// an empty string; non-octet prefixes round down ("/20" -> "a.b.").
[[nodiscard]] std::string subnetInputPrefix(std::string_view cidr);

// Remembers probe results so reopening Add Host does not probe again. A host
// stays known for the session; a "no host here" answer expires, so a PC that
// was asleep shows up again later.
class ProbeCache {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto kNegativeTtl = std::chrono::minutes(10);

    [[nodiscard]] std::optional<bool> lookup(const std::string& peerId,
                                             Clock::time_point now) const;
    void store(const std::string& peerId, bool isHost, Clock::time_point now);
    // Drops "no host" answers only (Refresh: recheck a PC that was asleep);
    // found hosts stay known.
    void forgetMisses();
    void clear();

private:
    struct Entry {
        bool isHost = false;
        Clock::time_point at{};
    };
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
};

} // namespace artemis::remote
