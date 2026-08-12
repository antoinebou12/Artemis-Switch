#pragma once

#include "HostKeyboardShortcuts.hpp"
#include "PointerSettings.hpp"

namespace artemis::input {

class InputSettingsStore {
public:
    static constexpr int SchemaVersion = 1;
    static InputSettingsStore& instance();

    const PointerSettings& pointer();
    const std::vector<KeyboardShortcut>& shortcuts();
    void setPointer(const PointerSettings& settings, bool persist = true);
    bool setShortcuts(std::vector<KeyboardShortcut> shortcuts,
                      std::string* error = nullptr);
    void reload();
    bool save() const;

private:
    void ensureLoaded();

    PointerSettings m_pointer;
    std::vector<KeyboardShortcut> m_shortcuts;
    bool m_loaded = false;
};

} // namespace artemis::input
