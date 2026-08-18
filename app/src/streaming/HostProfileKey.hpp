#pragma once

#include "Settings.hpp"
#include "HostRecordIdentity.hpp"
#include "UsableMac.hpp"

#include <string>

namespace artemis::streaming {

inline std::string host_profile_key(const Host& host) {
    return stable_host_profile_key(host.mac, {}, host.preferred_address());
}

inline std::string host_profile_key(const Host& host, const std::string& serverMac) {
    return stable_host_profile_key(host.mac, serverMac,
                                   host.preferred_address());
}

} // namespace artemis::streaming
