#include "ZoomPanStore.hpp"

#include "Settings.hpp"

#include <filesystem>
#include <jansson.h>

namespace artemis::video {
namespace {
std::filesystem::path storePath() {
    return std::filesystem::path(Settings::instance().working_dir()) /
           "artemis_zoom_pan.json";
}
}

ZoomPanStore& ZoomPanStore::instance() {
    static ZoomPanStore store;
    return store;
}

void ZoomPanStore::ensureLoaded() {
    if (!m_loaded)
        reload();
}

const ZoomPanOptions& ZoomPanStore::get() {
    ensureLoaded();
    return m_options;
}

void ZoomPanStore::setRemember(bool remember, bool persist) {
    ensureLoaded();
    m_options.rememberBetweenSessions = remember;
    if (!remember)
        m_options.state = {};
    if (persist)
        save();
}

void ZoomPanStore::setState(ZoomPanState state) {
    ensureLoaded();
    m_options.state = normalizeZoomPan(state);
    if (m_options.rememberBetweenSessions)
        save();
}

void ZoomPanStore::reset() {
    ensureLoaded();
    m_options.state = {};
    save();
}

void ZoomPanStore::reload() {
    m_options = {};
    m_loaded = true;

    json_error_t error{};
    json_t* root = json_load_file(storePath().string().c_str(), 0, &error);
    if (!root || !json_is_object(root)) {
        if (root) json_decref(root);
        return;
    }

    if (json_t* value = json_object_get(root, "remember"))
        m_options.rememberBetweenSessions = json_is_true(value);

    if (m_options.rememberBetweenSessions) {
        if (json_t* value = json_object_get(root, "zoom"); json_is_number(value))
            m_options.state.zoom = static_cast<float>(json_number_value(value));
        if (json_t* value = json_object_get(root, "pan_x"); json_is_number(value))
            m_options.state.panX = static_cast<float>(json_number_value(value));
        if (json_t* value = json_object_get(root, "pan_y"); json_is_number(value))
            m_options.state.panY = static_cast<float>(json_number_value(value));
        m_options.state = normalizeZoomPan(m_options.state);
    }

    json_decref(root);
}

bool ZoomPanStore::save() const {
    json_t* root = json_object();
    if (!root) return false;

    json_object_set_new(root, "remember",
                        m_options.rememberBetweenSessions ? json_true() : json_false());
    json_object_set_new(root, "zoom", json_real(m_options.state.zoom));
    json_object_set_new(root, "pan_x", json_real(m_options.state.panX));
    json_object_set_new(root, "pan_y", json_real(m_options.state.panY));

    const int result = json_dump_file(root, storePath().string().c_str(), JSON_INDENT(4));
    json_decref(root);
    return result == 0;
}

} // namespace artemis::video
