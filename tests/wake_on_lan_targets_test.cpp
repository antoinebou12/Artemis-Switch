#include "../app/src/features/host/WakeOnLanTargets.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

using artemis::host::buildMagicPacket;
using artemis::host::hasUsableOverrideMac;
using artemis::host::normalizeMacAddress;
using artemis::host::WakeOnLanOverride;
using artemis::host::wakeTargetAddresses;
using artemis::host::wakeTargetPorts;

namespace {

void macNormalization() {
    assert(normalizeMacAddress("AA:BB:CC:DD:EE:FF") == "AABBCCDDEEFF");
    assert(normalizeMacAddress("aa-bb-cc-dd-ee-ff") == "AABBCCDDEEFF");
    assert(normalizeMacAddress("aabbccddeeff") == "AABBCCDDEEFF");
    assert(normalizeMacAddress("00:00:00:00:00:00").empty());
    assert(normalizeMacAddress("000000000000").empty());
    assert(normalizeMacAddress("").empty());
    assert(normalizeMacAddress("AA:BB:CC:DD:EE").empty());
    assert(normalizeMacAddress("GG:HH:II:JJ:KK:LL").empty());
}

void overrideMacPredictate() {
    WakeOnLanOverride none{};
    assert(!hasUsableOverrideMac(none));
    WakeOnLanOverride zero;
    zero.mac = "00:00:00:00:00:00";
    assert(!hasUsableOverrideMac(zero));
    WakeOnLanOverride ok;
    ok.mac = "AA:BB:CC:DD:EE:FF";
    assert(hasUsableOverrideMac(ok));
}

void ports() {
    WakeOnLanOverride none{};
    // Default multiport fan-out: static ports + dynamic sunshine-relative.
    const auto def = wakeTargetPorts(none, 47989);
    assert(def.size() == 7);
    for (auto p : {9, 47009, 47998, 47999, 48000, 48002, 48010})
        assert(std::find(def.begin(), def.end(),
                         static_cast<unsigned short>(p)) != def.end());

    // Relative to a custom base port, the dynamic ports shift.
    const auto shifted = wakeTargetPorts(none, 49000);
    assert(shifted.size() == 7);
    assert(std::find(shifted.begin(), shifted.end(),
                     49000 + (47998 - 47989)) != shifted.end());

    // An explicit override port wins outright.
    WakeOnLanOverride single;
    single.port = 9;
    assert(wakeTargetPorts(single, 47989) ==
           std::vector<unsigned short>{9});
    single.port = 50000;
    assert(wakeTargetPorts(single, 47989) ==
           std::vector<unsigned short>{50000});
}

void magicPacket() {
    // 102-byte classic frame.
    const auto pkt = buildMagicPacket("AABBCCDDEEFF", "");
    assert(pkt.size() == 102);
    for (int i = 0; i < 6; ++i)
        assert(pkt[i] == 0xFF);
    const uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    for (int rep = 0; rep < 16; ++rep) {
        for (int j = 0; j < 6; ++j)
            assert(pkt[6 + rep * 6 + j] == mac[j]);
    }

    // Bad MAC produces an empty packet.
    assert(buildMagicPacket("", "").empty());
    assert(buildMagicPacket("000000000000", "").empty());
    assert(buildMagicPacket("AABBCC", "").empty());
}

void secureOn() {
    // SecureOn password appends its 6 bytes -> 108-byte frame.
    const auto pkt = buildMagicPacket("AABBCCDDEEFF", "001122334455");
    assert(pkt.size() == 108);
    const uint8_t pwd[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    for (int j = 0; j < 6; ++j)
        assert(pkt[102 + j] == pwd[j]);

    // An invalid password must not produce a malformed frame.
    assert(buildMagicPacket("AABBCCDDEEFF", "xyz").empty());
    assert(buildMagicPacket("AABBCCDDEEFF", "1234567890").empty());
}

void addressExpansion() {
    WakeOnLanOverride none{};
    const std::vector<std::string> hosts = {"192.168.1.100", "192.168.1.101"};
    auto addrs = wakeTargetAddresses("", none, hosts, true);
    assert(addrs == hosts);

    // Override address goes first and stays once.
    WakeOnLanOverride over;
    over.address = "sunshine.example.com:47989";
    addrs = wakeTargetAddresses("", over, hosts, true);
    assert(addrs.size() == 3);
    assert(addrs[0] == "sunshine.example.com:47989");
}

} // namespace

int main() {
    macNormalization();
    overrideMacPredictate();
    ports();
    magicPacket();
    secureOn();
    addressExpansion();
    return 0;
}