#pragma once

#include "ZoomPanState.hpp"

namespace artemis::video {

struct ZoomPanOptions {
    bool rememberBetweenSessions = false;
    ZoomPanState state;
};

class ZoomPanStore {
public:
    static ZoomPanStore& instance();

    const ZoomPanOptions& get();
    void setRemember(bool remember, bool persist = true);
    void setState(ZoomPanState state);
    void reset();
    void reload();
    bool save() const;

private:
    void ensureLoaded();

    ZoomPanOptions m_options;
    bool m_loaded = false;
};

} // namespace artemis::video
