#pragma once

#include "SwitchStreamProfile.hpp"

namespace artemis::streaming {

struct StoredStreamProfile {
    bool customResolutionEnabled = false;
    int width = 1920;
    int height = 1080;
};

class StreamProfileStore {
public:
    static StreamProfileStore& instance();

    const StoredStreamProfile& get();
    void setCustomResolution(bool enabled, int width, int height);
    void reload();
    bool save() const;

private:
    void ensureLoaded();

    StoredStreamProfile m_settings;
    bool m_loaded = false;
};

} // namespace artemis::streaming
