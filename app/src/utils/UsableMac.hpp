#pragma once

#include <cctype>
#include <string>

/**
 * Whether a MAC address identifies a host.
 *
 * Sunshine answers serverinfo with an all-zero MAC when it cannot determine
 * the real one. That is not an identity and not a usable Wake-on-LAN target.
 */
inline bool is_usable_mac(const std::string& mac) {
    for (unsigned char ch : mac) {
        if (ch == ':' || ch == '-' || ch == ' ')
            continue;
        if (!std::isxdigit(ch))
            return false;
        if (ch != '0')
            return true;
    }

    // Empty, or every digit was a zero.
    return false;
}

/** Lowercase hex digits only — for equality across AA:BB vs aa-bb forms. */
inline std::string normalize_mac_key(const std::string& mac) {
    std::string key;
    key.reserve(mac.size());
    for (unsigned char ch : mac) {
        if (ch == ':' || ch == '-' || ch == ' ')
            continue;
        key.push_back(static_cast<char>(std::tolower(ch)));
    }
    return key;
}
