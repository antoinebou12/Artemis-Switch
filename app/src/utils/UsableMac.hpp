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
