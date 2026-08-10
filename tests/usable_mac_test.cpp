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

    return 0;
}
