#include "WakeOnLanTargets.hpp"

#include <array>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>

namespace artemis::host {
namespace {

constexpr unsigned short kDefaultSunshinePort = 47989;
constexpr std::array<unsigned short, 2> kStaticPorts = {9, 47009};
constexpr std::array<unsigned short, 5> kDynamicPorts = {
    47998, 47999, 48000, 48002, 48010};

void pushUniqueString(std::vector<std::string>& out, const std::string& v) {
    if (!v.empty() &&
        std::find(out.begin(), out.end(), v) == out.end()) {
        out.push_back(v);
    }
}

void pushUniquePort(std::vector<unsigned short>& out, unsigned short p) {
    if (std::find(out.begin(), out.end(), p) == out.end()) {
        out.push_back(p);
    }
}

} // namespace

std::string normalizeMacAddress(const std::string& mac) {
    std::string normalized;
    normalized.reserve(mac.size());
    for (unsigned char ch : mac) {
        if (std::isxdigit(ch))
            normalized.push_back(static_cast<char>(std::toupper(ch)));
        else if (ch == ':' || ch == '-' || std::isspace(ch))
            continue;
        else
            return "";
    }
    if (normalized.size() != 12)
        return "";
    // All-zero MAC is reported when Sunshine cannot resolve the real one.
    if (normalized.find_first_not_of('0') == std::string::npos)
        return "";
    return normalized;
}

bool hasUsableOverrideMac(const WakeOnLanOverride& over) {
    return !normalizeMacAddress(over.mac).empty();
}

std::vector<std::string> wakeTargetAddresses(
    const std::string& storedMac, const WakeOnLanOverride& over,
    const std::vector<std::string>& hostAddresses, bool includeBroadcast) {
    std::vector<std::string> out;
    // An explicit override target is authoritative and goes first.
    pushUniqueString(out, over.address);
    for (const auto& a : hostAddresses)
        pushUniqueString(out, a);
    // The stored MAC an override may replace is not an address; it feeds
    // create_payload() separately. Broadcast expansion is Switch-network
    // specific and stays in WakeOnLanManager, so includeBroadcast is accepted
    // here only to keep the fan-out contract explicit.
    (void)storedMac;
    (void)includeBroadcast;
    return out;
}

std::vector<unsigned short> wakeTargetPorts(const WakeOnLanOverride& over,
                                            unsigned short basePort) {
    std::vector<unsigned short> out;
    if (over.port != 0) {
        out.push_back(over.port);
        return out;
    }
    for (const auto p : kStaticPorts)
        pushUniquePort(out, p);
    const unsigned short base = basePort != 0 ? basePort : kDefaultSunshinePort;
    for (const auto p : kDynamicPorts)
        pushUniquePort(out, static_cast<unsigned short>((p - kDefaultSunshinePort) + base));
    return out;
}

// Parses a 12-hex-char MAC string (already normalized uppercase, no
// separators) into 6 raw bytes. Returns true on success.
bool macToBytes(const std::string& normalized, std::uint8_t bytes[6]) {
    if (normalized.size() != 12)
        return false;
    for (int i = 0; i < 6; ++i) {
        const unsigned char hi = static_cast<unsigned char>(normalized[i * 2]);
        const unsigned char lo = static_cast<unsigned char>(normalized[i * 2 + 1]);
        auto nibble = [](unsigned char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        const int hiN = nibble(hi);
        const int loN = nibble(lo);
        if (hiN < 0 || loN < 0)
            return false;
        bytes[i] = static_cast<std::uint8_t>((hiN << 4) | loN);
    }
    return true;
}

std::vector<std::uint8_t> buildMagicPacket(
    const std::string& normalizedMac, const std::string& secureOnPassword) {
    const std::uint8_t* macBytes = nullptr;
    std::uint8_t mac[6] = {};
    if (!macToBytes(normalizeMacAddress(normalizedMac), mac))
        return {};
    macBytes = mac;

    std::vector<std::uint8_t> packet;
    packet.reserve(102 + 6);
    packet.insert(packet.end(), 6, 0xFF);
    for (int i = 0; i < 16; ++i)
        packet.insert(packet.end(), macBytes, macBytes + 6);

    if (!secureOnPassword.empty()) {
        std::uint8_t pwd[6] = {};
        if (!macToBytes(normalizeMacAddress(secureOnPassword), pwd))
            return {};
        packet.insert(packet.end(), pwd, pwd + 6);
    }
    if (packet.size() != 102 && packet.size() != 108)
        return {};
    return packet;
}

} // namespace artemis::host