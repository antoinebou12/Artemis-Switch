#include "StreamConfigProfileStore.hpp"

#include "StreamProfileStore.hpp"
#include "features/stream/AdvancedStreamOptionsStore.hpp"
#include "video/VideoScaleStore.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <jansson.h>
#include <random>

namespace artemis::streaming {
namespace {

std::filesystem::path storePath() {
    return std::filesystem::path(Settings::instance().working_dir()) /
           "artemis_profiles.json";
}

const char* scaleModeToString(artemis::video::ScaleMode mode) {
    switch (mode) {
    case artemis::video::ScaleMode::Fit:
        return "fit";
    case artemis::video::ScaleMode::Stretch:
        return "stretch";
    case artemis::video::ScaleMode::Fill:
    default:
        return "fill";
    }
}

artemis::video::ScaleMode scaleModeFromString(const char* value) {
    if (!value)
        return artemis::video::ScaleMode::Fill;
    if (std::string(value) == "fit")
        return artemis::video::ScaleMode::Fit;
    if (std::string(value) == "stretch")
        return artemis::video::ScaleMode::Stretch;
    return artemis::video::ScaleMode::Fill;
}

const char* codecToString(VideoCodec codec) {
    switch (codec) {
    case H264:
        return "H264";
    case AV1:
        return "AV1";
    case H265:
    default:
        return "H265";
    }
}

VideoCodec codecFromString(const char* value) {
    if (!value)
        return H264;
    const std::string s(value);
    if (s == "H264" || s == "h264")
        return H264;
    if (s == "AV1" || s == "av1")
        return AV1;
    return H265;
}

json_t* profileToJson(const StreamConfigProfile& profile) {
    json_t* item = json_object();
    if (!item)
        return nullptr;
    json_object_set_new(item, "id", json_string(profile.id.c_str()));
    json_object_set_new(item, "name", json_string(profile.name.c_str()));
    json_object_set_new(item, "resolution_height",
                        json_integer(profile.resolutionHeight));
    json_object_set_new(item, "fps", json_integer(profile.fps));
    json_object_set_new(item, "bitrate_kbps",
                        json_integer(profile.bitrateKbps));
    json_object_set_new(item, "video_codec",
                        json_string(codecToString(profile.videoCodec)));
    json_object_set_new(item, "stream_audio_configuration",
                        json_integer(static_cast<int>(
                            profile.streamAudioConfiguration)));
    json_object_set_new(item, "upscaling_mode",
                        json_integer(static_cast<int>(profile.upscalingMode)));
    json_object_set_new(item, "force_full_range_video",
                        profile.forceFullRangeVideo ? json_true()
                                                    : json_false());
    json_object_set_new(item, "prevent_packet_loss",
                        profile.preventPacketLoss ? json_true() : json_false());
    json_object_set_new(item, "scale_mode",
                        json_string(scaleModeToString(profile.scaleMode)));
    return item;
}

StreamConfigProfile profileFromJson(json_t* object) {
    StreamConfigProfile profile;
    if (!json_is_object(object))
        return profile;

    if (json_t* id = json_object_get(object, "id"); json_is_string(id))
        profile.id = json_string_value(id);
    if (json_t* name = json_object_get(object, "name"); json_is_string(name))
        profile.name = json_string_value(name);
    if (json_t* height = json_object_get(object, "resolution_height");
        json_is_integer(height))
        profile.resolutionHeight =
            static_cast<int>(json_integer_value(height));
    if (json_t* fps = json_object_get(object, "fps"); json_is_integer(fps))
        profile.fps = static_cast<int>(json_integer_value(fps));
    if (json_t* bitrate = json_object_get(object, "bitrate_kbps");
        json_is_integer(bitrate))
        profile.bitrateKbps = static_cast<int>(json_integer_value(bitrate));
    if (json_t* codec = json_object_get(object, "video_codec");
        json_is_string(codec))
        profile.videoCodec = codecFromString(json_string_value(codec));
    if (json_t* audio = json_object_get(object, "stream_audio_configuration");
        json_is_integer(audio))
        profile.streamAudioConfiguration =
            static_cast<StreamAudioConfiguration>(json_integer_value(audio));
    if (json_t* upscaling = json_object_get(object, "upscaling_mode");
        json_is_integer(upscaling))
        profile.upscalingMode =
            static_cast<UpscalingMode>(json_integer_value(upscaling));
    if (json_t* full = json_object_get(object, "force_full_range_video"))
        profile.forceFullRangeVideo = json_is_true(full);
    if (json_t* loss = json_object_get(object, "prevent_packet_loss"))
        profile.preventPacketLoss = json_is_true(loss);
    if (json_t* scale = json_object_get(object, "scale_mode");
        json_is_string(scale))
        profile.scaleMode = scaleModeFromString(json_string_value(scale));

    profile.resolutionHeight =
        StreamConfigProfileStore::normalizeHeight(profile.resolutionHeight);
    if (profile.fps <= 0)
        profile.fps = 60;
    if (profile.bitrateKbps <= 0)
        profile.bitrateKbps = 10000;
    if (profile.name.empty())
        profile.name = "Profile";
    return profile;
}

} // namespace

StreamConfigProfileStore& StreamConfigProfileStore::instance() {
    static StreamConfigProfileStore store;
    return store;
}

int StreamConfigProfileStore::normalizeHeight(int height) {
    constexpr int allowed[] = {360, 480, 720, 1080};
    int best = 720;
    int bestDistance = std::abs(height - best);
    for (int value : allowed) {
        const int distance = std::abs(height - value);
        if (distance < bestDistance) {
            best = value;
            bestDistance = distance;
        }
    }
    return best;
}

std::string StreamConfigProfileStore::makeId() {
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> dist;
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "p-%lld-%08x",
                  static_cast<long long>(now), dist(rng));
    return buffer;
}

void StreamConfigProfileStore::ensureLoaded() {
    if (!m_loaded)
        reload();
}

StreamConfigProfile
StreamConfigProfileStore::snapshotFromSettings(const std::string& name) {
    StreamConfigProfile profile;
    profile.id = makeId();
    profile.name = name.empty() ? "Profile" : name;
    profile.resolutionHeight =
        normalizeHeight(Settings::instance().resolution() > 0
                            ? Settings::instance().resolution()
                            : 720);
    profile.fps = Settings::instance().fps();
    profile.bitrateKbps = Settings::instance().bitrate();
    profile.videoCodec = Settings::instance().video_codec();
    profile.streamAudioConfiguration =
        Settings::instance().stream_audio_configuration();
    profile.upscalingMode = Settings::instance().upscaling_mode();

    const auto advanced =
        artemis::stream::AdvancedStreamOptionsStore::instance().get();
    profile.forceFullRangeVideo = advanced.forceFullRangeVideo;
    profile.preventPacketLoss = advanced.preventPacketLoss;
    profile.scaleMode = artemis::video::VideoScaleStore::instance().get();
    return profile;
}

void StreamConfigProfileStore::seedDefaultsIfEmpty() {
    if (!m_profiles.empty())
        return;

    auto makeSeed = [](const char* name, int height, int fps, int bitrate) {
        StreamConfigProfile profile;
        profile.id = makeId();
        profile.name = name;
        profile.resolutionHeight = height;
        profile.fps = fps;
        profile.bitrateKbps = bitrate;
        profile.videoCodec = H264;
        return profile;
    };

    m_profiles.push_back(makeSeed("720p 60", 720, 60, 10000));
    m_profiles.push_back(makeSeed("1080p 60", 1080, 60, 20000));
    m_profiles.push_back(makeSeed("360p remote", 360, 30, 2500));
    m_activeProfileId = m_profiles.front().id;
}

const std::vector<StreamConfigProfile>& StreamConfigProfileStore::list() {
    ensureLoaded();
    return m_profiles;
}

std::optional<StreamConfigProfile>
StreamConfigProfileStore::get(const std::string& id) {
    ensureLoaded();
    if (id.empty())
        return std::nullopt;
    for (const auto& profile : m_profiles) {
        if (profile.id == id)
            return profile;
    }
    return std::nullopt;
}

StreamConfigProfile
StreamConfigProfileStore::create(const std::string& name,
                                 bool fromCurrentSettings) {
    ensureLoaded();
    StreamConfigProfile profile =
        fromCurrentSettings ? snapshotFromSettings(name)
                            : StreamConfigProfile{};
    if (!fromCurrentSettings) {
        profile.id = makeId();
        profile.name = name.empty() ? "Profile" : name;
        profile.resolutionHeight = 720;
    } else {
        profile.name = name.empty() ? profile.name : name;
        if (profile.id.empty())
            profile.id = makeId();
    }
    m_profiles.push_back(profile);
    if (m_activeProfileId.empty())
        m_activeProfileId = profile.id;
    save();
    return profile;
}

bool StreamConfigProfileStore::rename(const std::string& id,
                                      const std::string& name) {
    ensureLoaded();
    if (name.empty())
        return false;
    for (auto& profile : m_profiles) {
        if (profile.id != id)
            continue;
        profile.name = name;
        save();
        return true;
    }
    return false;
}

bool StreamConfigProfileStore::update(const StreamConfigProfile& profile) {
    ensureLoaded();
    if (profile.id.empty())
        return false;
    for (auto& existing : m_profiles) {
        if (existing.id != profile.id)
            continue;
        existing = profile;
        existing.resolutionHeight = normalizeHeight(existing.resolutionHeight);
        if (existing.name.empty())
            existing.name = "Profile";
        save();
        return true;
    }
    return false;
}

bool StreamConfigProfileStore::remove(const std::string& id) {
    ensureLoaded();
    const auto it = std::remove_if(
        m_profiles.begin(), m_profiles.end(),
        [&](const StreamConfigProfile& profile) { return profile.id == id; });
    if (it == m_profiles.end())
        return false;
    m_profiles.erase(it, m_profiles.end());

    for (auto hostIt = m_hostProfile.begin(); hostIt != m_hostProfile.end();) {
        if (hostIt->second == id)
            hostIt = m_hostProfile.erase(hostIt);
        else
            ++hostIt;
    }
    if (m_activeProfileId == id)
        m_activeProfileId =
            m_profiles.empty() ? std::string{} : m_profiles.front().id;

    seedDefaultsIfEmpty();
    save();
    return true;
}

StreamConfigProfile
StreamConfigProfileStore::duplicate(const std::string& id,
                                    const std::string& newName) {
    ensureLoaded();
    auto existing = get(id);
    if (!existing)
        return create(newName.empty() ? "Profile" : newName, true);

    StreamConfigProfile copy = *existing;
    copy.id = makeId();
    copy.name = newName.empty() ? (existing->name + " copy") : newName;
    m_profiles.push_back(copy);
    save();
    return copy;
}

std::string
StreamConfigProfileStore::selectedForHost(const std::string& hostKey) {
    ensureLoaded();
    if (hostKey.empty())
        return {};
    const auto it = m_hostProfile.find(hostKey);
    if (it == m_hostProfile.end())
        return {};
    if (!get(it->second))
        return {};
    return it->second;
}

void StreamConfigProfileStore::setSelectedForHost(
    const std::string& hostKey, const std::string& profileId) {
    ensureLoaded();
    if (hostKey.empty())
        return;
    if (profileId.empty() || !get(profileId)) {
        m_hostProfile.erase(hostKey);
    } else {
        m_hostProfile[hostKey] = profileId;
    }
    save();
}

void StreamConfigProfileStore::clearSelectedForHost(
    const std::string& hostKey) {
    setSelectedForHost(hostKey, {});
}

std::string StreamConfigProfileStore::activeProfileId() {
    ensureLoaded();
    if (!m_activeProfileId.empty() && get(m_activeProfileId))
        return m_activeProfileId;
    if (!m_profiles.empty())
        return m_profiles.front().id;
    return {};
}

void StreamConfigProfileStore::setActiveProfileId(
    const std::string& profileId) {
    ensureLoaded();
    if (profileId.empty() || !get(profileId))
        return;
    m_activeProfileId = profileId;
    save();
}

bool StreamConfigProfileStore::applyToSettings(const std::string& id) {
    auto profile = get(id);
    if (!profile)
        return false;

    auto& settings = Settings::instance();
    settings.set_resolution(profile->resolutionHeight);
    settings.set_fps(profile->fps);
    settings.set_bitrate(profile->bitrateKbps);
    settings.set_video_codec(profile->videoCodec);
    settings.set_stream_audio_configuration(
        profile->streamAudioConfiguration);
    settings.set_upscaling_mode(profile->upscalingMode);
    settings.save();

    auto advanced =
        artemis::stream::AdvancedStreamOptionsStore::instance().get();
    advanced.forceFullRangeVideo = profile->forceFullRangeVideo;
    advanced.preventPacketLoss = profile->preventPacketLoss;
    artemis::stream::AdvancedStreamOptionsStore::instance().set(advanced);

    artemis::video::VideoScaleStore::instance().set(profile->scaleMode);

    StreamProfileStore::instance().setCustomResolution(
        true, profile->resolutionWidth(), profile->resolutionHeight);

    m_activeProfileId = profile->id;
    save();
    return true;
}

bool StreamConfigProfileStore::exportJson(const std::string& path) const {
    if (!m_loaded)
        const_cast<StreamConfigProfileStore*>(this)->ensureLoaded();

    json_t* root = json_object();
    json_t* profiles = json_array();
    json_t* hosts = json_object();
    if (!root || !profiles || !hosts) {
        if (root)
            json_decref(root);
        if (profiles)
            json_decref(profiles);
        if (hosts)
            json_decref(hosts);
        return false;
    }

    json_object_set_new(root, "schema_version", json_integer(SchemaVersion));
    for (const auto& profile : m_profiles)
        json_array_append_new(profiles, profileToJson(profile));
    json_object_set_new(root, "profiles", profiles);
    for (const auto& [key, value] : m_hostProfile)
        json_object_set_new(hosts, key.c_str(), json_string(value.c_str()));
    json_object_set_new(root, "host_profile", hosts);
    if (!m_activeProfileId.empty())
        json_object_set_new(root, "active_profile_id",
                            json_string(m_activeProfileId.c_str()));

    const int result =
        json_dump_file(root, path.c_str(), JSON_INDENT(4));
    json_decref(root);
    return result == 0;
}

bool StreamConfigProfileStore::importJson(const std::string& path,
                                          bool renameOnConflict,
                                          std::string* errorOut) {
    ensureLoaded();
    json_error_t error{};
    json_t* root = json_load_file(path.c_str(), 0, &error);
    if (!root || !json_is_object(root)) {
        if (root)
            json_decref(root);
        if (errorOut)
            *errorOut = error.text ? error.text : "Invalid JSON";
        return false;
    }

    json_t* profiles = json_object_get(root, "profiles");
    if (!json_is_array(profiles)) {
        json_decref(root);
        if (errorOut)
            *errorOut = "Missing profiles array";
        return false;
    }

    size_t index = 0;
    json_t* item = nullptr;
    json_array_foreach(profiles, index, item) {
        StreamConfigProfile profile = profileFromJson(item);
        if (profile.id.empty())
            profile.id = makeId();
        if (get(profile.id)) {
            if (!renameOnConflict)
                continue;
            profile.id = makeId();
            profile.name += " (import)";
        }
        m_profiles.push_back(profile);
    }

    if (json_t* hosts = json_object_get(root, "host_profile");
        json_is_object(hosts)) {
        const char* key = nullptr;
        json_t* value = nullptr;
        json_object_foreach(hosts, key, value) {
            if (!key || !json_is_string(value))
                continue;
            const std::string profileId = json_string_value(value);
            if (get(profileId))
                m_hostProfile[key] = profileId;
        }
    }

    if (json_t* active = json_object_get(root, "active_profile_id");
        json_is_string(active)) {
        const std::string activeId = json_string_value(active);
        if (get(activeId))
            m_activeProfileId = activeId;
    }

    json_decref(root);
    seedDefaultsIfEmpty();
    save();
    return true;
}

void StreamConfigProfileStore::reload() {
    m_profiles.clear();
    m_hostProfile.clear();
    m_activeProfileId.clear();
    m_loaded = true;

    json_error_t error{};
    json_t* root = json_load_file(storePath().string().c_str(), 0, &error);
    if (!root || !json_is_object(root)) {
        if (root)
            json_decref(root);
        seedDefaultsIfEmpty();
        save();
        return;
    }

    if (json_t* profiles = json_object_get(root, "profiles");
        json_is_array(profiles)) {
        size_t index = 0;
        json_t* item = nullptr;
        json_array_foreach(profiles, index, item) {
            StreamConfigProfile profile = profileFromJson(item);
            if (profile.id.empty())
                profile.id = makeId();
            m_profiles.push_back(profile);
        }
    }

    if (json_t* hosts = json_object_get(root, "host_profile");
        json_is_object(hosts)) {
        const char* key = nullptr;
        json_t* value = nullptr;
        json_object_foreach(hosts, key, value) {
            if (key && json_is_string(value))
                m_hostProfile[key] = json_string_value(value);
        }
    }

    if (json_t* active = json_object_get(root, "active_profile_id");
        json_is_string(active))
        m_activeProfileId = json_string_value(active);

    json_decref(root);
    seedDefaultsIfEmpty();
    if (m_activeProfileId.empty() || !get(m_activeProfileId))
        m_activeProfileId =
            m_profiles.empty() ? std::string{} : m_profiles.front().id;
}

bool StreamConfigProfileStore::save() const {
    return exportJson(storePath().string());
}

} // namespace artemis::streaming
