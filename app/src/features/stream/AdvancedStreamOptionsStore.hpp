#pragma once

#include "AdvancedStreamOptions.hpp"

namespace artemis::stream {

class AdvancedStreamOptionsStore {
public:
    static AdvancedStreamOptionsStore& instance();

    const AdvancedStreamOptions& get();
    void set(const AdvancedStreamOptions& options, bool persist = true);
    void reload();
    bool save() const;

private:
    void ensureLoaded();

    AdvancedStreamOptions m_options;
    bool m_loaded = false;
};

} // namespace artemis::stream
