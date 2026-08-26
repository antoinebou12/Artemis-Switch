#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace artemis::remote_access {

inline constexpr std::size_t kVpnConfigDisplayLimit = 64 * 1024;

enum class VpnConfigLoadStatus {
    Ok,
    Missing,
    Unreadable,
    Empty,
};

struct VpnConfigPreview {
    VpnConfigLoadStatus status = VpnConfigLoadStatus::Missing;
    std::string text;
    bool truncated = false;
};

std::string redactVpnConfig(std::string_view config);
VpnConfigPreview loadVpnConfigPreview(
    const std::string& path,
    std::size_t displayLimit = kVpnConfigDisplayLimit);

} // namespace artemis::remote_access
