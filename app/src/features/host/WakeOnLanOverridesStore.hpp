#pragma once

#include "WakeOnLanTargets.hpp"

#include <map>
#include <string>

namespace artemis::host {

// Per-host persistent Wake-on-LAN overrides, keyed by the stable host profile
// key (stored MAC). Lets a host that reports an all-zero MAC still be woken
// with an explicit MAC, or a WAN/DDNS target be named explicitly.
class WakeOnLanOverridesStore {
public:
    static constexpr int SchemaVersion = 1;
    static WakeOnLanOverridesStore& instance();

    WakeOnLanOverride get(const std::string& hostKey);
    void set(const std::string& hostKey, WakeOnLanOverride over);
    void clear(const std::string& hostKey);
    void reload();
    bool save() const;

private:
    void ensureLoaded();
    std::map<std::string, WakeOnLanOverride> m_overrides;
    bool m_loaded = false;
};

} // namespace artemis::host