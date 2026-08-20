#include "UsableMac.hpp"
#include "HostRecordIdentity.hpp"

#include <cassert>

int main() {
    assert(!is_usable_mac(""));
    assert(!is_usable_mac("00:00:00:00:00:00"));
    assert(!is_usable_mac("00-00-00-00-00-00"));
    assert(!is_usable_mac("000000000000"));
    assert(!is_usable_mac("00:00:00:00:00:0G"));

    assert(is_usable_mac("AA:BB:CC:DD:EE:FF"));
    assert(is_usable_mac("aa-bb-cc-dd-ee-ff"));
    assert(is_usable_mac("00:00:00:00:00:01"));
    assert(is_usable_mac("FF:FF:FF:FF:FF:FF"));
    assert(is_usable_mac("AABBCCDDEEFF"));

    assert(normalize_mac_key("AA:BB:CC:DD:EE:FF") == "aabbccddeeff");
    assert(normalize_mac_key("aa-bb-cc-dd-ee-ff") == "aabbccddeeff");
    assert(normalize_mac_key("02:1a:2b:3c:4d:5e") ==
           normalize_mac_key("02:1A:2B:3C:4D:5E"));

    assert(normalize_host_display_name("  GAMINGPC ") == "gamingpc");
    assert(hosts_share_display_name("GAMINGPC", " gamingpc "));
    assert(!hosts_share_display_name("", ""));

    assert(stable_host_profile_key("AA:BB:CC:DD:EE:FF",
                                   "11:22:33:44:55:66", "192.168.1.10") ==
           "AA:BB:CC:DD:EE:FF");
    assert(stable_host_profile_key("00:00:00:00:00:00",
                                   "11:22:33:44:55:66", "192.168.1.10") ==
           "11:22:33:44:55:66");
    assert(stable_host_profile_key("", "", "192.168.1.10") ==
           "192.168.1.10");

    return 0;
}
