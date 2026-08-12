#include "StreamConfigProfileStore.hpp"

#include "StreamConfigProfileNormalize.hpp"
#include "DefaultStreamProfiles.hpp"
#include "StreamProfileStore.hpp"
#include "features/input/InputSettingsStore.hpp"
#include "features/input/SwitchMotionPolicyStore.hpp"
#include "features/stream/AdvancedStreamOptionsStore.hpp"
#include "features/video/ZoomPanStore.hpp"
#include "video/VideoScaleStore.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <jansson.h>
#include <random>
#include <string>
#include <vector>

namespace artemis::streaming {

StreamConfigProfile normalizeProfile(StreamConfigProfile profile);

namespace {

std::filesystem::path storePath() {
    return std::filesystem::path(Settings::instance().working_dir()) /
           profileStoreFilename();
}

std::filesystem::path legacyStorePath() {
    return std::filesystem::path(Settings::instance().working_dir()) /
           legacyProfileStoreFilename();
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
    const std::string s(value);
    if (s == "fit")
        return artemis::video::ScaleMode::Fit;
    if (s == "stretch")
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

const char* audioBackendToString(AudioBackend backend) {
#ifdef __SWITCH__
    if (backend == AUDREN)
        return "audren";
#endif
    return "sdl";
}

AudioBackend audioBackendFromString(const char* value) {
#ifdef __SWITCH__
    if (value && std::string(value) == "audren")
        return AUDREN;
#endif
    (void)value;
    return SDL;
}

bool legacyTouchscreenFromPointerMode(artemis::input::PointerMode mode) {
    return mode == artemis::input::PointerMode::MultiTouch;
}

bool jsonToBool(json_t* value) {
    return value && json_is_true(value);
}

float jsonToFloat(json_t* value, float defaultValue) {
    if (!value)
        return defaultValue;
    if (json_is_real(value))
        return static_cast<float>(json_real_value(value));
    if (json_is_integer(value))
        return static_cast<float>(json_integer_value(value));
    return defaultValue;
}

json_t* profileToJson(const StreamConfigProfile& profile) {
    json_t* item = json_object();
    if (!item)
        return nullptr;

    json_object_set_new(item, "id", json_string(profile.id.c_str()));
    json_object_set_new(item, "name", json_string(profile.name.c_str()));

    json_object_set_new(item, "resolution_height",
                        json_integer(profile.resolutionHeight));
    json_object_set_new(item, "aspect_ratio",
                        json_string(aspectRatioToString(profile.aspectRatio)));
    json_object_set_new(item, "fps", json_integer(profile.fps));
    json_object_set_new(item, "client_refresh_rate_x100",
                        json_integer(profile.clientRefreshRateX100));
    json_object_set_new(item, "bitrate_kbps",
                        json_integer(profile.bitrateKbps));
    json_object_set_new(item, "video_codec",
                        json_string(codecToString(profile.videoCodec)));
    json_object_set_new(item, "request_hdr",
                        profile.requestHdr ? json_true() : json_false());
    json_object_set_new(item, "decoder_threads",
                        json_integer(profile.decoderThreads));
    json_object_set_new(item, "native_resolution_scale",
                        json_integer(profile.nativeResolutionScale));
    json_object_set_new(item, "use_hw_decoding",
                        profile.useHwDecoding ? json_true() : json_false());
    json_object_set_new(item, "force_full_range_video",
                        profile.forceFullRangeVideo ? json_true()
                                                    : json_false());
    json_object_set_new(item, "prevent_packet_loss",
                        profile.preventPacketLoss ? json_true() : json_false());
    json_object_set_new(item, "low_latency_pacing",
                        profile.lowLatencyPacing ? json_true() : json_false());
    json_object_set_new(item, "packet_size",
                        json_integer(artemis::stream::clampPacketSize(
                            profile.packetSize)));
    json_object_set_new(item, "custom_resolution_enabled",
                        profile.customResolutionEnabled ? json_true()
                                                        : json_false());
    json_object_set_new(item, "custom_width",
                        json_integer(profile.customWidth));
    json_object_set_new(item, "custom_height",
                        json_integer(profile.customHeight));

    json_object_set_new(item, "scale_mode",
                        json_string(scaleModeToString(profile.scaleMode)));
    json_object_set_new(item, "remember_zoom_pan",
                        profile.rememberZoomPan ? json_true() : json_false());
    json_object_set_new(item, "forward_motion",
                        profile.forwardMotion ? json_true() : json_false());
    json_object_set_new(
        item, "console_motion_fallback",
        profile.consoleMotionFallback ? json_true() : json_false());
    json_object_set_new(item, "upscaling_mode",
                        json_integer(static_cast<int>(profile.upscalingMode)));
    json_object_set_new(item, "dithering",
                        profile.dithering ? json_true() : json_false());
    json_object_set_new(item, "dithering_strength",
                        json_real(static_cast<double>(profile.ditheringStrength)));
    json_object_set_new(item, "rcas", profile.rcas ? json_true() : json_false());
    json_object_set_new(item, "rcas_strength",
                        json_real(static_cast<double>(profile.rcasStrength)));

    json_object_set_new(item, "stream_audio_configuration",
                        json_integer(static_cast<int>(
                            profile.streamAudioConfiguration)));
    json_object_set_new(item, "play_audio_on_pc",
                        profile.playAudioOnPc ? json_true() : json_false());
    json_object_set_new(item, "sops", profile.sops ? json_true() : json_false());
    json_object_set_new(
        item, "terminate_app_on_disconnect",
        profile.terminateAppOnDisconnect ? json_true() : json_false());
    json_object_set_new(item, "audio_backend",
                        json_string(audioBackendToString(profile.audioBackend)));
    json_object_set_new(item, "volume_amplification",
                        profile.volumeAmplification ? json_true()
                                                    : json_false());

    json_object_set_new(item, "keyboard_type",
                        json_integer(static_cast<int>(profile.keyboardType)));
    json_object_set_new(item, "keyboard_locale",
                        json_integer(profile.keyboardLocale));
    json_object_set_new(item, "keyboard_fingers",
                        json_integer(profile.keyboardFingers));

    json_object_set_new(
        item, "touchscreen_mouse_mode",
        profile.touchscreenMouseMode ? json_true() : json_false());
    json_object_set_new(
        item, "pointer_mode",
        json_string(artemis::input::pointerModeName(profile.pointerMode)));
    json_object_set_new(item, "swap_mouse_keys",
                        profile.swapMouseKeys ? json_true() : json_false());
    json_object_set_new(item, "swap_mouse_scroll",
                        profile.swapMouseScroll ? json_true() : json_false());
    json_object_set_new(item, "swap_mouse_sticks",
                        profile.swapMouseSticks ? json_true() : json_false());
    json_object_set_new(item, "mouse_speed_multiplier",
                        json_integer(profile.mouseSpeedMultiplier));

    json_object_set_new(item, "swap_ui_ab",
                        profile.swapUiAb ? json_true() : json_false());
    json_object_set_new(item, "swap_ui_xy",
                        profile.swapUiXy ? json_true() : json_false());
    json_object_set_new(item, "swap_ui_keys",
                        (profile.swapUiAb || profile.swapUiXy) ? json_true()
                                                               : json_false());
    json_object_set_new(item, "deadzone_left",
                        json_real(static_cast<double>(profile.deadzoneLeft)));
    json_object_set_new(item, "deadzone_right",
                        json_real(static_cast<double>(profile.deadzoneRight)));
    json_object_set_new(item, "rumble_force",
                        json_real(static_cast<double>(profile.rumbleForce)));
    json_object_set_new(item, "swap_stick_to_dpad",
                        profile.swapStickToDpad ? json_true() : json_false());
    json_object_set_new(item, "mapping_layout_title",
                        json_string(profile.mappingLayoutTitle.c_str()));

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
    if (json_t* aspect = json_object_get(object, "aspect_ratio")) {
        if (json_is_string(aspect))
            profile.aspectRatio =
                aspectRatioFromString(json_string_value(aspect));
        else if (json_is_integer(aspect))
            profile.aspectRatio = aspectRatioFromInt(
                static_cast<int>(json_integer_value(aspect)));
    }
    if (json_t* fps = json_object_get(object, "fps"); json_is_integer(fps))
        profile.fps = static_cast<int>(json_integer_value(fps));
    if (json_t* refreshX100 =
            json_object_get(object, "client_refresh_rate_x100");
        json_is_integer(refreshX100))
        profile.clientRefreshRateX100 =
            static_cast<int>(json_integer_value(refreshX100));
    if (json_t* bitrate = json_object_get(object, "bitrate_kbps");
        json_is_integer(bitrate))
        profile.bitrateKbps = static_cast<int>(json_integer_value(bitrate));
    if (json_t* codec = json_object_get(object, "video_codec");
        json_is_string(codec))
        profile.videoCodec = codecFromString(json_string_value(codec));
    if (json_t* hdr = json_object_get(object, "request_hdr"))
        profile.requestHdr = jsonToBool(hdr);
    if (json_t* threads = json_object_get(object, "decoder_threads");
        json_is_integer(threads))
        profile.decoderThreads = static_cast<int>(json_integer_value(threads));
    if (json_t* nativeScale = json_object_get(object, "native_resolution_scale");
        json_is_integer(nativeScale))
        profile.nativeResolutionScale =
            static_cast<int>(json_integer_value(nativeScale));
    if (json_t* hw = json_object_get(object, "use_hw_decoding"))
        profile.useHwDecoding = jsonToBool(hw);
    if (json_t* full = json_object_get(object, "force_full_range_video"))
        profile.forceFullRangeVideo = jsonToBool(full);
    if (json_t* loss = json_object_get(object, "prevent_packet_loss"))
        profile.preventPacketLoss = jsonToBool(loss);
    if (json_t* lowLatency = json_object_get(object, "low_latency_pacing"))
        profile.lowLatencyPacing = jsonToBool(lowLatency);
    if (json_t* packetSize = json_object_get(object, "packet_size");
        json_is_integer(packetSize))
        profile.packetSize = artemis::stream::clampPacketSize(
            static_cast<int>(json_integer_value(packetSize)));
    if (json_t* custom = json_object_get(object, "custom_resolution_enabled"))
        profile.customResolutionEnabled = jsonToBool(custom);
    if (json_t* customW = json_object_get(object, "custom_width");
        json_is_integer(customW))
        profile.customWidth = static_cast<int>(json_integer_value(customW));
    if (json_t* customH = json_object_get(object, "custom_height");
        json_is_integer(customH))
        profile.customHeight = static_cast<int>(json_integer_value(customH));

    if (json_t* scale = json_object_get(object, "scale_mode");
        json_is_string(scale))
        profile.scaleMode = scaleModeFromString(json_string_value(scale));
    if (json_t* remember = json_object_get(object, "remember_zoom_pan"))
        profile.rememberZoomPan = jsonToBool(remember);
    if (json_t* motion = json_object_get(object, "forward_motion"))
        profile.forwardMotion = jsonToBool(motion);
    if (json_t* console = json_object_get(object, "console_motion_fallback"))
        profile.consoleMotionFallback = jsonToBool(console);
    if (json_t* upscaling = json_object_get(object, "upscaling_mode");
        json_is_integer(upscaling))
        profile.upscalingMode =
            static_cast<UpscalingMode>(json_integer_value(upscaling));
    if (json_t* dithering = json_object_get(object, "dithering"))
        profile.dithering = jsonToBool(dithering);
    profile.ditheringStrength =
        jsonToFloat(json_object_get(object, "dithering_strength"),
                    profile.ditheringStrength);
    if (json_t* rcas = json_object_get(object, "rcas"))
        profile.rcas = jsonToBool(rcas);
    profile.rcasStrength =
        jsonToFloat(json_object_get(object, "rcas_strength"),
                    profile.rcasStrength);

    if (json_t* audio = json_object_get(object, "stream_audio_configuration");
        json_is_integer(audio))
        profile.streamAudioConfiguration =
            static_cast<StreamAudioConfiguration>(json_integer_value(audio));
    if (json_t* playAudio = json_object_get(object, "play_audio_on_pc"))
        profile.playAudioOnPc = jsonToBool(playAudio);
    if (json_t* sops = json_object_get(object, "sops"))
        profile.sops = jsonToBool(sops);
    if (json_t* terminate = json_object_get(object, "terminate_app_on_disconnect"))
        profile.terminateAppOnDisconnect = jsonToBool(terminate);
    if (json_t* audioBackend = json_object_get(object, "audio_backend");
        json_is_string(audioBackend))
        profile.audioBackend =
            audioBackendFromString(json_string_value(audioBackend));
    else if (json_t* audioBackendInt = json_object_get(object, "audio_backend");
             json_is_integer(audioBackendInt))
        profile.audioBackend = static_cast<AudioBackend>(
            json_integer_value(audioBackendInt));
    if (json_t* amp = json_object_get(object, "volume_amplification"))
        profile.volumeAmplification = jsonToBool(amp);

    if (json_t* keyboardType = json_object_get(object, "keyboard_type");
        json_is_integer(keyboardType))
        profile.keyboardType =
            static_cast<KeyboardType>(json_integer_value(keyboardType));
    if (json_t* keyboardLocale = json_object_get(object, "keyboard_locale");
        json_is_integer(keyboardLocale))
        profile.keyboardLocale =
            static_cast<int>(json_integer_value(keyboardLocale));
    if (json_t* keyboardFingers = json_object_get(object, "keyboard_fingers");
        json_is_integer(keyboardFingers))
        profile.keyboardFingers =
            static_cast<int>(json_integer_value(keyboardFingers));

    if (json_t* touchMouse = json_object_get(object, "touchscreen_mouse_mode"))
        profile.touchscreenMouseMode = jsonToBool(touchMouse);
    bool hasPointerMode = false;
    if (json_t* pointerMode = json_object_get(object, "pointer_mode");
        json_is_string(pointerMode)) {
        artemis::input::PointerMode mode{};
        if (artemis::input::pointerModeFromName(json_string_value(pointerMode),
                                                mode)) {
            profile.pointerMode = mode;
            hasPointerMode = true;
        }
    }
    if (!hasPointerMode)
        profile.pointerMode = artemis::input::pointerModeFromLegacyTouchscreen(
            profile.touchscreenMouseMode);
    profile.touchscreenMouseMode =
        legacyTouchscreenFromPointerMode(profile.pointerMode);
    if (json_t* swapKeys = json_object_get(object, "swap_mouse_keys"))
        profile.swapMouseKeys = jsonToBool(swapKeys);
    if (json_t* swapScroll = json_object_get(object, "swap_mouse_scroll"))
        profile.swapMouseScroll = jsonToBool(swapScroll);
    if (json_t* swapSticks = json_object_get(object, "swap_mouse_sticks"))
        profile.swapMouseSticks = jsonToBool(swapSticks);
    if (json_t* mouseSpeed = json_object_get(object, "mouse_speed_multiplier");
        json_is_integer(mouseSpeed))
        profile.mouseSpeedMultiplier =
            static_cast<int>(json_integer_value(mouseSpeed));

    if (json_t* swapUi = json_object_get(object, "swap_ui_keys")) {
        const bool value = jsonToBool(swapUi);
        profile.swapUiAb = value;
        profile.swapUiXy = value;
    }
    if (json_t* swapUiAb = json_object_get(object, "swap_ui_ab"))
        profile.swapUiAb = jsonToBool(swapUiAb);
    if (json_t* swapUiXy = json_object_get(object, "swap_ui_xy"))
        profile.swapUiXy = jsonToBool(swapUiXy);
    profile.deadzoneLeft =
        jsonToFloat(json_object_get(object, "deadzone_left"),
                    profile.deadzoneLeft);
    profile.deadzoneRight =
        jsonToFloat(json_object_get(object, "deadzone_right"),
                    profile.deadzoneRight);
    profile.rumbleForce =
        jsonToFloat(json_object_get(object, "rumble_force"),
                    profile.rumbleForce);
    if (json_t* swapStick = json_object_get(object, "swap_stick_to_dpad"))
        profile.swapStickToDpad = jsonToBool(swapStick);
    if (json_t* mappingTitle =
            json_object_get(object, "mapping_layout_title");
        json_is_string(mappingTitle))
        profile.mappingLayoutTitle = json_string_value(mappingTitle);

    return normalizeProfile(profile);
}

void applyProfileToSettings(const StreamConfigProfile& profile, bool persist) {
    auto& settings = Settings::instance();
    settings.set_resolution(profile.resolutionHeight);
    settings.set_aspect_ratio(profile.aspectRatio);
    settings.set_fps(profile.fps);
    settings.set_client_refresh_rate_x100(profile.clientRefreshRateX100);
    settings.set_bitrate(profile.bitrateKbps);
    settings.set_video_codec(profile.videoCodec);
    settings.set_request_hdr(profile.requestHdr);
    settings.set_decoder_threads(profile.decoderThreads);
    settings.set_native_resolution_scale(profile.nativeResolutionScale);
    settings.set_use_hw_decoding(profile.useHwDecoding);
    settings.set_upscaling_mode(profile.upscalingMode);
    settings.set_dithering(profile.dithering);
    settings.set_dithering_strength(profile.ditheringStrength);
    settings.set_rcas(profile.rcas);
    settings.set_rcas_strength(profile.rcasStrength);
    settings.set_stream_audio_configuration(profile.streamAudioConfiguration);
    settings.set_play_audio(profile.playAudioOnPc);
    settings.set_sops(profile.sops);
    settings.set_terminate_app_on_disconnect(profile.terminateAppOnDisconnect);
    settings.set_audio_backend(profile.audioBackend);
    settings.set_volume_amplification(profile.volumeAmplification);
    settings.set_keyboard_type(profile.keyboardType);
    settings.set_keyboard_locale(profile.keyboardLocale);
    settings.set_keyboard_fingers(profile.keyboardFingers);
    settings.set_touchscreen_mouse_mode(profile.touchscreenMouseMode);
    settings.set_swap_mouse_keys(profile.swapMouseKeys);
    settings.set_swap_mouse_scroll(profile.swapMouseScroll);
    settings.set_swap_mouse_sticks(profile.swapMouseSticks);
    settings.set_mouse_speed_multiplier(profile.mouseSpeedMultiplier);
    settings.set_swap_ui_ab(profile.swapUiAb);
    settings.set_swap_ui_xy(profile.swapUiXy);
    brls::Application::setSwapABInputKeys(profile.swapUiAb);
    brls::Application::setSwapXYInputKeys(profile.swapUiXy);
    settings.set_deadzone_stick_left(profile.deadzoneLeft);
    settings.set_deadzone_stick_right(profile.deadzoneRight);
    settings.set_rumble_force(profile.rumbleForce);
    settings.set_swap_joycon_stick_to_dpad(profile.swapStickToDpad);
    settings.set_low_latency_pacing(profile.lowLatencyPacing);
    if (!profile.mappingLayoutTitle.empty()) {
        auto* layouts = settings.get_mapping_laouts();
        for (size_t i = 0; layouts && i < layouts->size(); ++i) {
            if ((*layouts)[i].title == profile.mappingLayoutTitle) {
                settings.set_current_mapping_layout(static_cast<int>(i));
                break;
            }
        }
    }
    if (persist)
        settings.save();

    auto advanced =
        artemis::stream::AdvancedStreamOptionsStore::instance().get();
    advanced.forceFullRangeVideo = profile.forceFullRangeVideo;
    advanced.preventPacketLoss = profile.preventPacketLoss;
    advanced.packetSize =
        artemis::stream::clampPacketSize(profile.packetSize);
    artemis::stream::AdvancedStreamOptionsStore::instance().set(advanced,
                                                                persist);

    artemis::video::VideoScaleStore::instance().set(profile.scaleMode, persist);
    artemis::video::ZoomPanStore::instance().setRemember(profile.rememberZoomPan,
                                                        persist);

    auto motion = artemis::input::SwitchMotionPolicyStore::instance().get();
    motion.allowGamepadMotionSensors = profile.forwardMotion;
    motion.allowConsoleMotionFallback = profile.consoleMotionFallback;
    artemis::input::SwitchMotionPolicyStore::instance().set(motion, persist);

    const int customW = profile.customResolutionEnabled
                            ? profile.customWidth
                            : profile.resolutionWidth();
    const int customH = profile.customResolutionEnabled
                            ? profile.customHeight
                            : profile.resolutionHeight;
    StreamProfileStore::instance().setCustomResolution(
        profile.customResolutionEnabled, customW, customH, persist);

    auto pointer = artemis::input::InputSettingsStore::instance().pointer();
    pointer.mode = profile.pointerMode;
    artemis::input::InputSettingsStore::instance().setPointer(pointer, persist);
}

} // namespace

StreamConfigProfile normalizeProfile(StreamConfigProfile profile) {
    profile.resolutionHeight = normalizeProfileHeight(profile.resolutionHeight);
    profile.aspectRatio = normalizeAspectRatio(profile.aspectRatio);
    if (profile.fps <= 0)
        profile.fps = 60;
    if (profile.clientRefreshRateX100 < 0)
        profile.clientRefreshRateX100 = 0;
    if (profile.bitrateKbps <= 0)
        profile.bitrateKbps = 10000;
    if (profile.decoderThreads < 0)
        profile.decoderThreads = 4;
    if (profile.name.empty())
        profile.name = "Profile";

    profile.nativeResolutionScale =
        normalizeNativeResolutionScale(profile.nativeResolutionScale);
    profile.customWidth =
        normalizeCustomDimension(profile.customWidth, 1920);
    profile.customHeight =
        normalizeCustomDimension(profile.customHeight, 1080);

    profile.ditheringStrength =
        std::clamp(profile.ditheringStrength, 1.0f, 10.0f);
    profile.rcasStrength = std::clamp(profile.rcasStrength, 0.0f, 1.0f);
    profile.deadzoneLeft = std::clamp(profile.deadzoneLeft, 0.0f, 1.0f);
    profile.deadzoneRight = std::clamp(profile.deadzoneRight, 0.0f, 1.0f);
    profile.rumbleForce = std::clamp(profile.rumbleForce, 0.0f, 1.0f);
    profile.mouseSpeedMultiplier =
        std::clamp(profile.mouseSpeedMultiplier, 0, 100);
    profile.packetSize = artemis::stream::clampPacketSize(profile.packetSize);
    profile.keyboardFingers = std::clamp(profile.keyboardFingers, 0, 10);
    if (profile.keyboardLocale < 0)
        profile.keyboardLocale = 0;

    profile.touchscreenMouseMode =
        legacyTouchscreenFromPointerMode(profile.pointerMode);

    return profile;
}

StreamConfigProfileStore& StreamConfigProfileStore::instance() {
    static StreamConfigProfileStore store;
    return store;
}

int StreamConfigProfileStore::normalizeHeight(int height) {
    return normalizeProfileHeight(height);
}

std::string StreamConfigProfileStore::makeId() {
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> dist;
    const auto now =
        std::chrono::steady_clock::now().time_since_epoch().count();
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

    const auto& settings = Settings::instance();
    profile.resolutionHeight = normalizeHeight(settings.resolution());
    profile.aspectRatio = settings.aspect_ratio();
    profile.fps = settings.fps();
    profile.clientRefreshRateX100 = settings.client_refresh_rate_x100();
    profile.bitrateKbps = settings.bitrate();
    profile.videoCodec = settings.video_codec();
    profile.requestHdr = settings.request_hdr();
    profile.decoderThreads = settings.decoder_threads();
    profile.nativeResolutionScale = settings.native_resolution_scale();
    profile.useHwDecoding = settings.use_hw_decoding();
    profile.upscalingMode = settings.upscaling_mode();
    profile.dithering = settings.dithering();
    profile.ditheringStrength = settings.dithering_strength();
    profile.rcas = settings.rcas();
    profile.rcasStrength = settings.rcas_strength();
    profile.streamAudioConfiguration = settings.stream_audio_configuration();
    profile.playAudioOnPc = settings.play_audio();
    profile.sops = settings.sops();
    profile.terminateAppOnDisconnect = settings.terminate_app_on_disconnect();
    profile.audioBackend = settings.audio_backend();
    profile.volumeAmplification = settings.get_volume_amplification();
    profile.keyboardType = settings.get_keyboard_type();
    profile.keyboardLocale = settings.get_keyboard_locale();
    profile.keyboardFingers = settings.get_keyboard_fingers();
    profile.pointerMode =
        artemis::input::InputSettingsStore::instance().pointer().mode;
    profile.touchscreenMouseMode =
        legacyTouchscreenFromPointerMode(profile.pointerMode);
    profile.swapMouseKeys = settings.swap_mouse_keys();
    profile.swapMouseScroll = settings.swap_mouse_scroll();
    profile.swapMouseSticks = settings.swap_mouse_sticks();
    profile.mouseSpeedMultiplier = settings.get_mouse_speed_multiplier();
    profile.swapUiAb = settings.swap_ui_ab();
    profile.swapUiXy = settings.swap_ui_xy();
    profile.deadzoneLeft = settings.get_deadzone_stick_left();
    profile.deadzoneRight = settings.get_deadzone_stick_right();
    profile.rumbleForce = settings.get_rumble_force();
    profile.swapStickToDpad = settings.swap_joycon_stick_to_dpad();
    {
        auto& mutableSettings = Settings::instance();
        const auto* layouts = mutableSettings.get_mapping_laouts();
        const int index = mutableSettings.get_current_mapping_layout();
        if (layouts && index >= 0 &&
            index < static_cast<int>(layouts->size()))
            profile.mappingLayoutTitle =
                (*layouts)[static_cast<size_t>(index)].title;
    }

    const auto advanced =
        artemis::stream::AdvancedStreamOptionsStore::instance().get();
    profile.forceFullRangeVideo = advanced.forceFullRangeVideo;
    profile.preventPacketLoss = advanced.preventPacketLoss;
    profile.lowLatencyPacing = settings.low_latency_pacing();
    profile.packetSize =
        artemis::stream::clampPacketSize(advanced.packetSize);
    profile.scaleMode = artemis::video::VideoScaleStore::instance().get();
    profile.rememberZoomPan =
        artemis::video::ZoomPanStore::instance().get().rememberBetweenSessions;
    {
        const auto motion =
            artemis::input::SwitchMotionPolicyStore::instance().get();
        profile.forwardMotion = motion.allowGamepadMotionSensors;
        profile.consoleMotionFallback = motion.allowConsoleMotionFallback;
    }

    const auto custom = StreamProfileStore::instance().get();
    profile.customResolutionEnabled = custom.customResolutionEnabled;
    profile.customWidth = custom.width;
    profile.customHeight = custom.height;

    return normalizeProfile(profile);
}

int StreamConfigProfileStore::addMissingDefaults() {
    ensureLoaded();
    std::vector<std::string> names;
    names.reserve(m_profiles.size());
    for (const auto& profile : m_profiles)
        names.push_back(profile.name);

    int added = 0;
    for (const auto& spec : missingDefaultProfiles(names)) {
        StreamConfigProfile profile;
        profile.id = makeId();
        profile.name = spec.name;
        profile.resolutionHeight = spec.height;
        profile.aspectRatio = StreamAspectRatio::Ratio16x9;
        profile.fps = spec.fps;
        profile.bitrateKbps = spec.bitrateKbps;
        profile.videoCodec = H264;
        profile.scaleMode = artemis::video::ScaleMode::Fill;
        m_profiles.push_back(profile);
        ++added;
    }
    if (m_activeProfileId.empty() && !m_profiles.empty()) {
        for (const auto& profile : m_profiles) {
            if (profile.name == kDefaultActiveProfileName) {
                m_activeProfileId = profile.id;
                break;
            }
        }
        if (m_activeProfileId.empty())
            m_activeProfileId = m_profiles.front().id;
    }
    if (added > 0)
        save();
    return added;
}

void StreamConfigProfileStore::seedDefaultsIfEmpty() {
    if (!m_profiles.empty())
        return;
    addMissingDefaults();
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
    profile = normalizeProfile(profile);
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
        existing = normalizeProfile(profile);
        existing.id = profile.id;
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

    // Do not re-seed here: empty after delete must stay empty so remove works.
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

bool StreamConfigProfileStore::applyProfile(const StreamConfigProfile& profile,
                                            bool persist) {
    applyProfileToSettings(normalizeProfile(profile), persist);
    return true;
}

bool StreamConfigProfileStore::applyToSettings(const std::string& id,
                                               bool persist) {
    auto profile = get(id);
    if (!profile)
        return false;
    if (!applyProfile(*profile, persist))
        return false;
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

    const int result = json_dump_file(root, path.c_str(), JSON_INDENT(4));
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
    save();
    return true;
}

void StreamConfigProfileStore::reload() {
    m_profiles.clear();
    m_hostProfile.clear();
    m_activeProfileId.clear();
    m_loaded = true;

    json_error_t error{};
    const auto primary = storePath();
    json_t* root = json_load_file(primary.string().c_str(), 0, &error);
    bool migratedFromLegacy = false;
    if (!root || !json_is_object(root)) {
        if (root)
            json_decref(root);
        root = nullptr;
        const auto legacy = legacyStorePath();
        std::error_code ec;
        if (std::filesystem::exists(legacy, ec)) {
            root = json_load_file(legacy.string().c_str(), 0, &error);
            migratedFromLegacy = root && json_is_object(root);
            if (!migratedFromLegacy && root) {
                json_decref(root);
                root = nullptr;
            }
        }
    }
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
    if (m_activeProfileId.empty() || !get(m_activeProfileId))
        m_activeProfileId =
            m_profiles.empty() ? std::string{} : m_profiles.front().id;
    if (migratedFromLegacy)
        save();
}

bool StreamConfigProfileStore::save() const {
    return exportJson(storePath().string());
}

} // namespace artemis::streaming
