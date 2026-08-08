#include "StreamProfileStore.hpp"

#include "Settings.hpp"

#include <filesystem>
#include <jansson.h>

namespace artemis::streaming {
namespace {
std::filesystem::path storePath() {
    return std::filesystem::path(Settings::instance().working_dir()) /
           "artemis_stream.json";
}
}

StreamProfileStore& StreamProfileStore::instance() {
    static StreamProfileStore store;
    return store;
}

void StreamProfileStore::ensureLoaded() {
    if (!m_loaded)
        reload();
}

const StoredStreamProfile& StreamProfileStore::get() {
    ensureLoaded();
    return m_settings;
}

void StreamProfileStore::setCustomResolution(bool enabled, int width, int height) {
    ensureLoaded();

    StreamProfile profile;
    profile.width = width;
    profile.height = height;
    profile.fps = Settings::instance().fps();
    profile.bitrateKbps = Settings::instance().bitrate();
    profile.decoderThreads = Settings::instance().decoder_threads();
    profile.codec = Settings::instance().video_codec() == H264 ? "H264" : "HEVC";
    profile = SwitchStreamProfile::normalized(profile);

    m_settings.customResolutionEnabled = enabled;
    m_settings.width = profile.width;
    m_settings.height = profile.height;
    save();
}

void StreamProfileStore::reload() {
    m_settings = {};
    m_loaded = true;

    json_error_t error{};
    json_t* root = json_load_file(storePath().string().c_str(), 0, &error);
    if (!root || !json_is_object(root)) {
        if (root)
            json_decref(root);
        return;
    }

    if (json_t* enabled = json_object_get(root, "custom_resolution_enabled"))
        m_settings.customResolutionEnabled = json_is_true(enabled);
    if (json_t* width = json_object_get(root, "width"); json_is_integer(width))
        m_settings.width = static_cast<int>(json_integer_value(width));
    if (json_t* height = json_object_get(root, "height"); json_is_integer(height))
        m_settings.height = static_cast<int>(json_integer_value(height));

    StreamProfile normalized;
    normalized.width = m_settings.width;
    normalized.height = m_settings.height;
    normalized = SwitchStreamProfile::normalized(normalized);
    m_settings.width = normalized.width;
    m_settings.height = normalized.height;

    json_decref(root);
}

bool StreamProfileStore::save() const {
    json_t* root = json_object();
    if (!root)
        return false;

    json_object_set_new(root, "custom_resolution_enabled",
                        m_settings.customResolutionEnabled ? json_true() : json_false());
    json_object_set_new(root, "width", json_integer(m_settings.width));
    json_object_set_new(root, "height", json_integer(m_settings.height));

    const int result = json_dump_file(root, storePath().string().c_str(), JSON_INDENT(4));
    json_decref(root);
    return result == 0;
}

} // namespace artemis::streaming
