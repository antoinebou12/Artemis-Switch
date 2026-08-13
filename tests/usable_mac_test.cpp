#include "UsableMac.hpp"

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
    assert(normalize_mac_key("60:ff:9e:09:9f:ba") ==
           normalize_mac_key("60:FF:9E:09:9F:BA"));

    return 0;
}
