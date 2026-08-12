#pragma once

#include "Settings.hpp"
#include "StreamAspectRatio.hpp"
#include "StreamConfigProfileNormalize.hpp"
#include "features/input/PointerSettings.hpp"
#include "video/VideoScale.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace artemis::streaming {

struct StreamConfigProfile {
    std::string id;
    std::string name;

    // Video
    int resolutionHeight = 720; // -1 native, or 360 / 480 / 540 / 720 / 1080 / 1440
    StreamAspectRatio aspectRatio = StreamAspectRatio::Ratio16x9;
    int fps = 60;
    // 0 = fps * 100. NTSC presets store 5994 / 11988 / etc.
    int clientRefreshRateX100 = 0;
    int bitrateKbps = 10000;
    VideoCodec videoCodec = H264;
    bool requestHdr = false;
    int decoderThreads = 4;
    int nativeResolutionScale = 100;
    bool useHwDecoding = true;
    bool forceFullRangeVideo = false;
    bool preventPacketLoss = false;
    bool lowLatencyPacing = false;
    // 0 = Auto (see artemis::stream::resolveStreamPacketSize).
    int packetSize = 0;
    bool customResolutionEnabled = false;
    int customWidth = 1920;
    int customHeight = 1080;

    // Presentation
    artemis::video::ScaleMode scaleMode = artemis::video::ScaleMode::Fill;
    bool rememberZoomPan = false;
    UpscalingMode upscalingMode = UPSCALING_OFF;
    bool dithering = false;
    float ditheringStrength = 3.0f;
    bool rcas = true;
    float rcasStrength = 0.2f;
    // 0 = Off. Named presets write upscalingMode + rcas* in the editor.
    int fsrPreset = 0;

    // Motion (Artemis Settings)
    bool forwardMotion = true;
    bool consoleMotionFallback = false;

    // Audio / stream
    StreamAudioConfiguration streamAudioConfiguration = STREAM_AUDIO_STEREO;
    bool playAudioOnPc = false;
    bool sops = false;
    bool terminateAppOnDisconnect = false;
#ifdef __SWITCH__
    AudioBackend audioBackend = AUDREN;
#else
    AudioBackend audioBackend = SDL;
#endif
    bool volumeAmplification = false;

    // Keyboard
    KeyboardType keyboardType = COMPACT;
    int keyboardLocale = 0;
    int keyboardFingers = 3;

    // Mouse / pointer
    bool touchscreenMouseMode = false;
    artemis::input::PointerMode pointerMode =
        artemis::input::PointerMode::TrackpadNatural;
    bool swapMouseKeys = false;
    bool swapMouseScroll = false;
    bool swapMouseSticks = false;
    int mouseSpeedMultiplier = 47;

    // Controller
    bool swapUiAb = false;
    bool swapUiXy = false;
    float deadzoneLeft = 0.0f;
    float deadzoneRight = 0.0f;
    float rumbleForce = 1.0f;
    bool swapStickToDpad = false;
    // Empty = leave current Settings layout unchanged on apply.
    std::string mappingLayoutTitle;

    [[nodiscard]] int resolutionWidth() const {
        return streamWidthFromHeight(resolutionHeight, aspectRatio);
    }
};

class StreamConfigProfileStore {
public:
    static constexpr int SchemaVersion = 12;
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

    static StreamConfigProfile snapshotFromSettings(const std::string& name);
    // persist=false applies in memory for the current stream only.
    bool applyToSettings(const std::string& id, bool persist = true);
    bool applyProfile(const StreamConfigProfile& profile, bool persist = true);

    bool exportJson(const std::string& path) const;
    bool importJson(const std::string& path, bool renameOnConflict = true,
                    std::string* errorOut = nullptr);

    // Insert built-in presets whose names are not already present.
    int addMissingDefaults();

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

StreamConfigProfile normalizeProfile(StreamConfigProfile profile);

} // namespace artemis::streaming
