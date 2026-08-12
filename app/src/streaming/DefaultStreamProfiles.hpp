#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

namespace artemis::streaming {

struct DefaultStreamProfileSpec {
    const char* name;
    int height;
    int fps;
    int bitrateKbps;
};

// Built-in presets: 30/60 FPS only. Seeds use 16:9; the editor can set 4:3.
inline constexpr std::array<DefaultStreamProfileSpec, 8> kDefaultStreamProfiles{{
    {"360p 30 0.5M", 360, 30, 500},
    {"360p 30 1M", 360, 30, 1000},
    {"720p 30 10M", 720, 30, 10000},
    {"720p 60 10M", 720, 60, 10000},
    {"1080p 30 20M", 1080, 30, 20000},
    {"1080p 60 20M", 1080, 60, 20000},
    {"1080p 60 50M", 1080, 60, 50000},
    {"1080p 60 100M", 1080, 60, 100000},
}};

inline constexpr const char* kDefaultActiveProfileName = "720p 60 10M";

inline bool isDefaultProfileName(std::string_view name) {
    for (const auto& spec : kDefaultStreamProfiles) {
        if (name == spec.name)
            return true;
    }
    return false;
}

template <typename NameRange>
inline std::vector<DefaultStreamProfileSpec>
missingDefaultProfiles(const NameRange& existingNames) {
    std::vector<DefaultStreamProfileSpec> missing;
    missing.reserve(kDefaultStreamProfiles.size());
    for (const auto& spec : kDefaultStreamProfiles) {
        bool exists = false;
        for (const auto& name : existingNames) {
            if (name == spec.name) {
                exists = true;
                break;
            }
        }
        if (!exists)
            missing.push_back(spec);
    }
    return missing;
}

} // namespace artemis::streaming
