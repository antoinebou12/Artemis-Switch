#pragma once

#include "VideoScale.hpp"

namespace artemis::video {

class VideoScaleStore {
public:
    static VideoScaleStore& instance();

    ScaleMode get();
    void set(ScaleMode mode, bool persist = true);
    void reload();
    bool save() const;

private:
    void ensureLoaded();

    ScaleMode m_mode = ScaleMode::Fill;
    bool m_loaded = false;
};

const char* scaleModeName(ScaleMode mode);

} // namespace artemis::video
