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

StoredStreamProfile normalizeProfile(StoredStreamProfile profile) {
    StreamProfile normalized;
    normalized.width = profile.width;
    normalized.height = profile.height;
    normalized.fps = Settings::instance().fps();
    normalized.bitrateKbps = Settings::instance().bitrate();
    normalized.decoderThreads = Settings::instance().decoder_threads();
    normalized.codec = Settings::instance().video_codec() == H264
                           ? "H264"
                           : (Settings::instance().video_codec() == AV1 ? "AV1"
                                                                       : "HEVC");
    normalized = SwitchStreamProfile::normalized(normalized);
    profile.width = normalized.width;
    profile.height = normalized.height;
    return profile;
}
} // namespace

StreamProfileStore& StreamProfileStore::instance() {
    static StreamProfileStore store;
    return store;
}

void StreamProfileStore::ensureLoaded() {
    if (!m_loaded)
        reload();
}

const StoredStreamProfile& StreamProfileStore::get(const std::string& hostKey) {
    ensureLoaded();
    if (hostKey.empty())
        return m_default;
    if (const auto it = m_hosts.find(hostKey); it != m_hosts.end())
        return it->second;
    return m_default;
}

void StreamProfileStore::setCustomResolution(const std::string& hostKey,
                                             bool enabled, int width,
                                             int height, bool persist) {
    ensureLoaded();
    StoredStreamProfile profile;
    profile.customResolutionEnabled = enabled;
    profile.width = width;
    profile.height = height;
    profile = normalizeProfile(profile);
    if (hostKey.empty()) {
        m_default = profile;
    } else {
        m_hosts[hostKey] = profile;
    }
    if (persist)
        save();
}

void StreamProfileStore::reload() {
    m_hosts.clear();
    m_default = {};
    m_loaded = true;

    json_error_t error{};
    json_t* root = json_load_file(storePath().string().c_str(), 0, &error);
    if (!root || !json_is_object(root)) {
        if (root)
            json_decref(root);
        return;
    }

    auto readProfile = [](json_t* object) {
        StoredStreamProfile profile;
        if (!json_is_object(object))
            return profile;
        if (json_t* enabled = json_object_get(object, "custom_resolution_enabled"))
            profile.customResolutionEnabled = json_is_true(enabled);
        if (json_t* width = json_object_get(object, "width");
            json_is_integer(width))
            profile.width = static_cast<int>(json_integer_value(width));
        if (json_t* height = json_object_get(object, "height");
            json_is_integer(height))
            profile.height = static_cast<int>(json_integer_value(height));
        return normalizeProfile(profile);
    };

    json_t* version = json_object_get(root, "schema_version");
    json_t* hosts = json_object_get(root, "hosts");
    if (json_is_integer(version) &&
        json_integer_value(version) >= SchemaVersion && json_is_object(hosts)) {
        if (json_t* defaults = json_object_get(root, "default"))
            m_default = readProfile(defaults);
        const char* key = nullptr;
        json_t* item = nullptr;
        json_object_foreach(hosts, key, item) {
            if (key)
                m_hosts[key] = readProfile(item);
        }
    } else {
        // Migrate legacy flat profile into the default slot.
        m_default = readProfile(root);
    }

    json_decref(root);
}

bool StreamProfileStore::save() const {
    json_t* root = json_object();
    json_t* hosts = json_object();
    if (!root || !hosts) {
        if (root)
            json_decref(root);
        if (hosts)
            json_decref(hosts);
        return false;
    }

    auto writeProfile = [](const StoredStreamProfile& profile) {
        json_t* item = json_object();
        json_object_set_new(item, "custom_resolution_enabled",
                            profile.customResolutionEnabled ? json_true()
                                                            : json_false());
        json_object_set_new(item, "width", json_integer(profile.width));
        json_object_set_new(item, "height", json_integer(profile.height));
        return item;
    };

    json_object_set_new(root, "schema_version", json_integer(SchemaVersion));
    json_object_set_new(root, "default", writeProfile(m_default));
    for (const auto& [key, profile] : m_hosts)
        json_object_set_new(hosts, key.c_str(), writeProfile(profile));
    json_object_set_new(root, "hosts", hosts);

    const int result =
        json_dump_file(root, storePath().string().c_str(), JSON_INDENT(4));
    json_decref(root);
    return result == 0;
}

} // namespace artemis::streaming
