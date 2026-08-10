#pragma once

#include "Settings.hpp"
#include "video/VideoScale.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace artemis::streaming {

struct StreamConfigProfile {
    std::string id;
    std::string name;
    int resolutionHeight = 720; // 360, 480, 720, or 1080
    int fps = 60;
    int bitrateKbps = 10000;
    VideoCodec videoCodec = H264;
    StreamAudioConfiguration streamAudioConfiguration = STREAM_AUDIO_STEREO;
    UpscalingMode upscalingMode = UPSCALING_OFF;
    bool forceFullRangeVideo = false;
    bool preventPacketLoss = false;
    artemis::video::ScaleMode scaleMode = artemis::video::ScaleMode::Fill;

    [[nodiscard]] int resolutionWidth() const {
        return resolutionHeight * 16 / 9;
    }
};

class StreamConfigProfileStore {
public:
    static constexpr int SchemaVersion = 1;
    static StreamConfigProfileStore& instance();

    const std::vector<StreamConfigProfile>& list();
    std::optional<StreamConfigProfile> get(const std::string& id);
    StreamConfigProfile create(const std::string& name,
                               bool fromCurrentSettings = true);
    bool rename(const std::string& id, const std::string& name);
    bool update(const StreamConfigProfile& profile);
    bool remove(const std::string& id);
    StreamConfigProfile duplicate(const std::string& id,
                                  const std::string& newName);

    std::string selectedForHost(const std::string& hostKey);
    void setSelectedForHost(const std::string& hostKey,
                            const std::string& profileId);
    void clearSelectedForHost(const std::string& hostKey);

    std::string activeProfileId();
    void setActiveProfileId(const std::string& profileId);

    // Snapshot current Settings / advanced options into a profile struct.
    static StreamConfigProfile snapshotFromSettings(const std::string& name);

    // Apply profile fields into Settings + related stores. Returns false if
    // the profile id is missing.
    bool applyToSettings(const std::string& id);

    bool exportJson(const std::string& path) const;
    // Merge by id; conflicting ids keep existing and import under a new id
    // when renameOnConflict is true (default).
    bool importJson(const std::string& path, bool renameOnConflict = true,
                    std::string* errorOut = nullptr);

    void reload();
    bool save() const;

    static int normalizeHeight(int height);

private:
    void ensureLoaded();
    void seedDefaultsIfEmpty();
    static std::string makeId();

    std::vector<StreamConfigProfile> m_profiles;
    std::map<std::string, std::string> m_hostProfile;
    std::string m_activeProfileId;
    bool m_loaded = false;
};

} // namespace artemis::streaming
