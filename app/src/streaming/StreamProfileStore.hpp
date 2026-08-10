#pragma once

#include "SwitchStreamProfile.hpp"

#include <map>
#include <string>

namespace artemis::streaming {

struct StoredStreamProfile {
    bool customResolutionEnabled = false;
    int width = 1920;
    int height = 1080;
};

class StreamProfileStore {
public:
    static constexpr int SchemaVersion = 2;
    static StreamProfileStore& instance();

    // Empty hostKey keeps the global Artemis settings default profile.
    const StoredStreamProfile& get(const std::string& hostKey = {});
    void setCustomResolution(const std::string& hostKey, bool enabled,
                             int width, int height);
    // Backward-compatible global setter used by Artemis settings.
    void setCustomResolution(bool enabled, int width, int height) {
        setCustomResolution({}, enabled, width, height);
    }
    void reload();
    bool save() const;

private:
    void ensureLoaded();

    std::map<std::string, StoredStreamProfile> m_hosts;
    StoredStreamProfile m_default;
    bool m_loaded = false;
};

} // namespace artemis::streaming
