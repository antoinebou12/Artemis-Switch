#pragma once

#include "DisplayTransform.hpp"

namespace artemis::video {

class DisplayTransformStore {
public:
    static constexpr int SchemaVersion = 1;
    static DisplayTransformStore& instance();

    DisplayTransform get();
    void set(DisplayTransform transform);
    void setRotation(Rotation rotation);
    void reload();
    bool save() const;

private:
    void ensureLoaded();
    DisplayTransform m_transform;
    bool m_loaded = false;
};

} // namespace artemis::video
