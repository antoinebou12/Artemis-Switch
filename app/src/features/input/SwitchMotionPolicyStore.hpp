#pragma once

#include "SwitchMotionPolicy.hpp"

namespace artemis::input {

class SwitchMotionPolicyStore {
public:
    static SwitchMotionPolicyStore& instance();

    const SwitchMotionOptions& get();
    void set(const SwitchMotionOptions& options);
    void reload();
    bool save() const;

private:
    void ensureLoaded();

    SwitchMotionOptions m_options;
    bool m_loaded = false;
};

} // namespace artemis::input
