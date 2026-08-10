#pragma once

#include "Settings.hpp"
#include "UsableMac.hpp"

#include <string>

namespace artemis::streaming {

inline std::string host_profile_key(const Host& host) {
    if (is_usable_mac(host.mac))
        return host.mac;
    return host.preferred_address();
}

inline std::string host_profile_key(const Host& host, const std::string& serverMac) {
    if (is_usable_mac(serverMac))
        return serverMac;
    return host_profile_key(host);
}

} // namespace artemis::streaming
