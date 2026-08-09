#include "VideoScaleStore.hpp"

#include "Settings.hpp"

#include <filesystem>
#include <jansson.h>

namespace artemis::video {
namespace {
std::filesystem::path storePath() {
    return std::filesystem::path(Settings::instance().working_dir()) /
           "artemis_video_scale.json";
}

ScaleMode parseMode(const char* value) {
    if (!value) return ScaleMode::Fill;
    const std::string text(value);
    if (text == "fit") return ScaleMode::Fit;
    if (text == "stretch") return ScaleMode::Stretch;
    return ScaleMode::Fill;
}
}

const char* scaleModeName(ScaleMode mode) {
    switch (mode) {
    case ScaleMode::Fit: return "Fit";
    case ScaleMode::Stretch: return "Stretch";
    case ScaleMode::Fill:
    default: return "Fill";
    }
}

VideoScaleStore& VideoScaleStore::instance() {
    static VideoScaleStore store;
    return store;
}

void VideoScaleStore::ensureLoaded() {
    if (!m_loaded)
        reload();
}

ScaleMode VideoScaleStore::get() {
    ensureLoaded();
    return m_mode;
}

void VideoScaleStore::set(ScaleMode mode) {
    m_mode = mode;
    m_loaded = true;
    save();
}

void VideoScaleStore::reload() {
    m_mode = ScaleMode::Fill;
    m_loaded = true;

    json_error_t error{};
    json_t* root = json_load_file(storePath().string().c_str(), 0, &error);
    if (!root || !json_is_object(root)) {
        if (root) json_decref(root);
        return;
    }

    if (json_t* mode = json_object_get(root, "mode"); json_is_string(mode))
        m_mode = parseMode(json_string_value(mode));
    json_decref(root);
}

bool VideoScaleStore::save() const {
    json_t* root = json_object();
    if (!root) return false;

    const char* mode = "fill";
    if (m_mode == ScaleMode::Fit) mode = "fit";
    else if (m_mode == ScaleMode::Stretch) mode = "stretch";
    json_object_set_new(root, "mode", json_string(mode));

    const int result = json_dump_file(root, storePath().string().c_str(), JSON_INDENT(4));
    json_decref(root);
    return result == 0;
}

} // namespace artemis::video
