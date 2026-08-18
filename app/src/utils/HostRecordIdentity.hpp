#pragma once

#include "UsableMac.hpp"

#include <cctype>
#include <string>
#include <string_view>

inline std::string normalize_host_display_name(std::string_view name) {
    std::size_t first = 0;
    while (first < name.size() &&
           std::isspace(static_cast<unsigned char>(name[first])))
        ++first;

    std::size_t last = name.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(name[last - 1])))
        --last;

    std::string key;
    key.reserve(last - first);
    for (std::size_t i = first; i < last; ++i) {
        key.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(name[i]))));
    }
    return key;
}

inline bool hosts_share_display_name(std::string_view lhs,
                                     std::string_view rhs) {
    const auto left = normalize_host_display_name(lhs);
    return !left.empty() && left == normalize_host_display_name(rhs);
}

inline std::string stable_host_profile_key(std::string_view storedMac,
                                           std::string_view serverMac,
                                           std::string_view address) {
    // Once a host has a persisted identity, keep profile assignments attached
    // to it even if a server update reports a differently formatted/new MAC.
    if (is_usable_mac(std::string(storedMac)))
        return std::string(storedMac);
    if (is_usable_mac(std::string(serverMac)))
        return std::string(serverMac);
    return std::string(address);
}
