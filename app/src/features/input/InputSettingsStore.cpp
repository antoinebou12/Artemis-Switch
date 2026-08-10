#include "InputSettingsStore.hpp"

#include "Settings.hpp"

#include <filesystem>
#include <jansson.h>

namespace artemis::input {
namespace {

std::filesystem::path storePath() {
    return std::filesystem::path(Settings::instance().working_dir()) /
           "artemis_input.json";
}

double number(json_t* object, const char* name, double fallback) {
    json_t* value = json_object_get(object, name);
    return json_is_number(value) ? json_number_value(value) : fallback;
}

bool boolean(json_t* object, const char* name, bool fallback) {
    json_t* value = json_object_get(object, name);
    return json_is_boolean(value) ? json_is_true(value) : fallback;
}

bool atomicDump(json_t* root, const std::filesystem::path& path) {
    const auto temporary = path.string() + ".tmp";
    if (json_dump_file(root, temporary.c_str(), JSON_INDENT(4) | JSON_SORT_KEYS) != 0)
        return false;

    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (!error) return true;

    const auto backup = path.string() + ".bak";
    std::filesystem::remove(backup, error);
    error.clear();
    if (std::filesystem::exists(path)) {
        std::filesystem::rename(path, backup, error);
        if (error) return false;
    }
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::error_code ignored;
        if (std::filesystem::exists(backup))
            std::filesystem::rename(backup, path, ignored);
        return false;
    }
    std::filesystem::remove(backup, error);
    return true;
}

} // namespace

InputSettingsStore& InputSettingsStore::instance() {
    static InputSettingsStore store;
    return store;
}

void InputSettingsStore::ensureLoaded() {
    if (!m_loaded) reload();
}

const PointerSettings& InputSettingsStore::pointer() {
    ensureLoaded();
    return m_pointer;
}

const std::vector<KeyboardShortcut>& InputSettingsStore::shortcuts() {
    ensureLoaded();
    return m_shortcuts;
}

void InputSettingsStore::setPointer(const PointerSettings& settings) {
    m_pointer = validatePointerSettings(settings);
    m_loaded = true;
    save();
}

bool InputSettingsStore::setShortcuts(std::vector<KeyboardShortcut> shortcuts,
                                      std::string* error) {
    for (const auto& shortcut : shortcuts) {
        const auto validation = validateShortcut(shortcut);
        if (!validation.valid) {
            if (error) *error = validation.error;
            return false;
        }
    }
    m_shortcuts = std::move(shortcuts);
    m_loaded = true;
    return save();
}

void InputSettingsStore::reload() {
    m_pointer = {};
    m_shortcuts.clear();
    m_loaded = true;

    const auto path = storePath();
    if (!std::filesystem::exists(path)) {
        m_pointer.mode = pointerModeFromLegacyTouchscreen(
            Settings::instance().touchscreen_mouse_mode());
        return;
    }

    json_error_t error{};
    json_t* root = json_load_file(path.string().c_str(), 0, &error);
    if (!root || !json_is_object(root)) {
        if (root) json_decref(root);
        return;
    }
    json_t* version = json_object_get(root, "schema_version");
    if (!json_is_integer(version) || json_integer_value(version) != SchemaVersion) {
        json_decref(root);
        return;
    }

    if (json_t* pointer = json_object_get(root, "pointer"); json_is_object(pointer)) {
        if (json_t* mode = json_object_get(pointer, "mode"); json_is_string(mode))
            pointerModeFromName(json_string_value(mode), m_pointer.mode);
        m_pointer.localCursor = boolean(pointer, "local_cursor", m_pointer.localCursor);
        m_pointer.naturalScrolling = boolean(pointer, "natural_scrolling", m_pointer.naturalScrolling);
        m_pointer.tapToClick = boolean(pointer, "tap_to_click", m_pointer.tapToClick);
        m_pointer.twoFingerRightClick = boolean(pointer, "two_finger_right_click", m_pointer.twoFingerRightClick);
        m_pointer.sensitivityX = number(pointer, "sensitivity_x", m_pointer.sensitivityX);
        m_pointer.sensitivityY = number(pointer, "sensitivity_y", m_pointer.sensitivityY);
        m_pointer.scrollSensitivity = number(pointer, "scroll_sensitivity", m_pointer.scrollSensitivity);
        m_pointer.dragThreshold = number(pointer, "drag_threshold", m_pointer.dragThreshold);
        if (json_t* region = json_object_get(pointer, "region"); json_is_object(region)) {
            m_pointer.region.left = number(region, "left", m_pointer.region.left);
            m_pointer.region.top = number(region, "top", m_pointer.region.top);
            m_pointer.region.right = number(region, "right", m_pointer.region.right);
            m_pointer.region.bottom = number(region, "bottom", m_pointer.region.bottom);
        }
        m_pointer = validatePointerSettings(m_pointer);
    }

    if (json_t* shortcuts = json_object_get(root, "shortcuts"); json_is_array(shortcuts)) {
        size_t index;
        json_t* item;
        json_array_foreach(shortcuts, index, item) {
            if (!json_is_object(item)) continue;
            KeyboardShortcut shortcut;
            json_t* id = json_object_get(item, "id");
            json_t* name = json_object_get(item, "name");
            json_t* keys = json_object_get(item, "keys");
            if (!json_is_string(id) || !json_is_string(name) || !json_is_array(keys))
                continue;
            shortcut.id = json_string_value(id);
            shortcut.name = json_string_value(name);
            shortcut.enabled = boolean(item, "enabled", true);
            size_t keyIndex;
            json_t* key;
            json_array_foreach(keys, keyIndex, key) {
                if (!json_is_string(key)) { shortcut.keys.clear(); break; }
                const auto parsed = virtualKeyFromSymbol(json_string_value(key));
                if (!parsed) { shortcut.keys.clear(); break; }
                shortcut.keys.push_back(*parsed);
            }
            if (validateShortcut(shortcut).valid) m_shortcuts.push_back(std::move(shortcut));
        }
    }
    json_decref(root);
}

bool InputSettingsStore::save() const {
    json_t* root = json_object();
    json_t* pointer = json_object();
    json_t* region = json_object();
    json_t* shortcuts = json_array();
    if (!root || !pointer || !region || !shortcuts) {
        if (root) json_decref(root);
        if (pointer) json_decref(pointer);
        if (region) json_decref(region);
        if (shortcuts) json_decref(shortcuts);
        return false;
    }

    json_object_set_new(root, "schema_version", json_integer(SchemaVersion));
    json_object_set_new(pointer, "mode", json_string(pointerModeName(m_pointer.mode)));
    json_object_set_new(pointer, "local_cursor", json_boolean(m_pointer.localCursor));
    json_object_set_new(pointer, "natural_scrolling", json_boolean(m_pointer.naturalScrolling));
    json_object_set_new(pointer, "tap_to_click", json_boolean(m_pointer.tapToClick));
    json_object_set_new(pointer, "two_finger_right_click", json_boolean(m_pointer.twoFingerRightClick));
    json_object_set_new(pointer, "sensitivity_x", json_real(m_pointer.sensitivityX));
    json_object_set_new(pointer, "sensitivity_y", json_real(m_pointer.sensitivityY));
    json_object_set_new(pointer, "scroll_sensitivity", json_real(m_pointer.scrollSensitivity));
    json_object_set_new(pointer, "drag_threshold", json_real(m_pointer.dragThreshold));
    json_object_set_new(region, "left", json_real(m_pointer.region.left));
    json_object_set_new(region, "top", json_real(m_pointer.region.top));
    json_object_set_new(region, "right", json_real(m_pointer.region.right));
    json_object_set_new(region, "bottom", json_real(m_pointer.region.bottom));
    json_object_set_new(pointer, "region", region);
    json_object_set_new(root, "pointer", pointer);

    for (const auto& shortcut : m_shortcuts) {
        if (!validateShortcut(shortcut).valid) continue;
        json_t* item = json_object();
        json_t* keys = json_array();
        json_object_set_new(item, "id", json_string(shortcut.id.c_str()));
        json_object_set_new(item, "name", json_string(shortcut.name.c_str()));
        json_object_set_new(item, "enabled", json_boolean(shortcut.enabled));
        for (const short key : shortcut.keys) {
            const auto symbol = symbolFromVirtualKey(key);
            if (symbol) json_array_append_new(keys, json_string(symbol->c_str()));
        }
        json_object_set_new(item, "keys", keys);
        json_array_append_new(shortcuts, item);
    }
    json_object_set_new(root, "shortcuts", shortcuts);
    const bool result = atomicDump(root, storePath());
    json_decref(root);
    return result;
}

} // namespace artemis::input
