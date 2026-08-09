#include "artemis_settings_tab.hpp"

#include "Settings.hpp"
#include "streaming/StreamProfileStore.hpp"

#if __has_include("features/stream/AdvancedStreamOptionsStore.hpp")
#include "features/stream/AdvancedStreamOptionsStore.hpp"
#define ARTEMIS_HAS_ADVANCED_STREAM 1
#else
#define ARTEMIS_HAS_ADVANCED_STREAM 0
#endif

#if __has_include("video/VideoScaleStore.hpp")
#include "video/VideoScaleStore.hpp"
#define ARTEMIS_HAS_VIDEO_SCALE 1
#else
#define ARTEMIS_HAS_VIDEO_SCALE 0
#endif

#if __has_include("features/input/SwitchMotionPolicyStore.hpp")
#include "features/input/SwitchMotionPolicyStore.hpp"
#define ARTEMIS_HAS_MOTION_POLICY 1
#else
#define ARTEMIS_HAS_MOTION_POLICY 0
#endif

#if __has_include("features/video/ZoomPanStore.hpp")
#include "features/video/ZoomPanStore.hpp"
#define ARTEMIS_HAS_ZOOM_PAN 1
#else
#define ARTEMIS_HAS_ZOOM_PAN 0
#endif

#include <algorithm>
#include <cstdlib>
#include <fmt/format.h>
#include <vector>

using namespace brls;

namespace {
std::vector<int> activeFrameRates() {
#if ARTEMIS_HAS_ADVANCED_STREAM
    return artemis::stream::availableFrameRates(
        artemis::stream::AdvancedStreamOptionsStore::instance().get());
#else
    return {30, 40, 60, 120};
#endif
}

std::vector<std::string> frameRateLabels(const std::vector<int>& values) {
    std::vector<std::string> labels;
    labels.reserve(values.size());
    for (const int value : values)
        labels.push_back(fmt::format("{} FPS", value));
    return labels;
}

int frameRateSelection(const std::vector<int>& values, int current) {
    const auto it = std::find(values.begin(), values.end(), current);
    if (it != values.end())
        return static_cast<int>(std::distance(values.begin(), it));

    int best = 0;
    int bestDistance = std::abs(values.front() - current);
    for (size_t i = 1; i < values.size(); ++i) {
        const int distance = std::abs(values[i] - current);
        if (distance < bestDistance) {
            best = static_cast<int>(i);
            bestDistance = distance;
        }
    }
    return best;
}
}

ArtemisSettingsTab::ArtemisSettingsTab() {
    inflateFromXMLRes("xml/tabs/artemis_settings.xml");

    const auto stored = artemis::streaming::StreamProfileStore::instance().get();
    customResolution->init("artemis/settings/use_custom_resolution"_i18n,
                           stored.customResolutionEnabled,
                           [this](bool enabled) {
        const auto value = artemis::streaming::StreamProfileStore::instance().get();
        artemis::streaming::StreamProfileStore::instance().setCustomResolution(
            enabled, value.width, value.height);
        refreshValues();
    });

    width->setText("artemis/settings/custom_width"_i18n);
    height->setText("artemis/settings/custom_height"_i18n);
    exactBitrate->setText("artemis/settings/exact_bitrate"_i18n);
    activeProfile->setText("artemis/settings/active_profile"_i18n);

    width->registerClickAction([this](View*) {
        editWidth();
        return true;
    });
    height->registerClickAction([this](View*) {
        editHeight();
        return true;
    });
    exactBitrate->registerClickAction([this](View*) {
        editBitrate();
        return true;
    });

    frameRate->setText("artemis/settings/stream_frame_rate"_i18n);
    refreshFrameRateSelector();

#if ARTEMIS_HAS_ADVANCED_STREAM
    const auto advanced = artemis::stream::AdvancedStreamOptionsStore::instance().get();
    unlockHighFps->init("artemis/settings/unlock_high_fps"_i18n,
                        advanced.unlockAllFrameRates,
                        [this](bool enabled) {
        auto options = artemis::stream::AdvancedStreamOptionsStore::instance().get();
        options.unlockAllFrameRates = enabled;
        artemis::stream::AdvancedStreamOptionsStore::instance().set(options);
        if (!enabled) {
            const int normalized = artemis::stream::normalizeFrameRate(
                Settings::instance().fps(), options);
            Settings::instance().set_fps(normalized);
            Settings::instance().save();
        }
        refreshFrameRateSelector();
        refreshValues();
    });
    forceFullRange->init("artemis/settings/force_full_range"_i18n,
                         advanced.forceFullRangeVideo, [](bool enabled) {
        auto options = artemis::stream::AdvancedStreamOptionsStore::instance().get();
        options.forceFullRangeVideo = enabled;
        artemis::stream::AdvancedStreamOptionsStore::instance().set(options);
    });
    forceFullRange->setDetailText(
        "Requests full range from the host on the next stream start/restart");
    preventPacketLoss->init("artemis/settings/prevent_packet_loss"_i18n,
                            advanced.preventPacketLoss, [](bool enabled) {
        auto options = artemis::stream::AdvancedStreamOptionsStore::instance().get();
        options.preventPacketLoss = enabled;
        artemis::stream::AdvancedStreamOptionsStore::instance().set(options);
    });
    preventPacketLoss->setDetailText("artemis/settings/packet_loss_unverified"_i18n);
#else
    unlockHighFps->init("artemis/settings/unlock_high_fps"_i18n, false, [](bool) {});
    forceFullRange->init("artemis/settings/force_full_range"_i18n, false, [](bool) {});
    preventPacketLoss->init("artemis/settings/prevent_packet_loss"_i18n, false, [](bool) {});
    unlockHighFps->setEnabled(false);
    forceFullRange->setEnabled(false);
    preventPacketLoss->setEnabled(false);
#endif

#if ARTEMIS_HAS_VIDEO_SCALE
    const auto currentScale = artemis::video::VideoScaleStore::instance().get();
    const int scaleSelection = currentScale == artemis::video::ScaleMode::Fit ? 0
                             : currentScale == artemis::video::ScaleMode::Fill ? 1 : 2;
    scaleMode->init("artemis/settings/video_scale_mode"_i18n,
                    {"artemis/settings/fit"_i18n,
                     "artemis/settings/fill"_i18n,
                     "artemis/settings/stretch"_i18n},
                    scaleSelection,
                    [](int selected) {
        const auto mode = selected == 0 ? artemis::video::ScaleMode::Fit
                        : selected == 2 ? artemis::video::ScaleMode::Stretch
                                        : artemis::video::ScaleMode::Fill;
        artemis::video::VideoScaleStore::instance().set(mode);
    });
#else
    scaleMode->init("artemis/settings/video_scale_mode"_i18n,
                    {"artemis/settings/fill"_i18n}, 0, [](int) {});
    scaleMode->setEnabled(false);
#endif

#if ARTEMIS_HAS_MOTION_POLICY
    auto motion = artemis::input::SwitchMotionPolicyStore::instance().get();
    forwardMotion->init("artemis/settings/forward_motion"_i18n,
                        motion.allowGamepadMotionSensors, [](bool enabled) {
        auto options = artemis::input::SwitchMotionPolicyStore::instance().get();
        options.allowGamepadMotionSensors = enabled;
        artemis::input::SwitchMotionPolicyStore::instance().set(options);
    });

    const auto capabilities = artemis::input::detectSwitchMotionCapabilities();
    const bool consoleFallbackSupported =
        artemis::input::canEnableConsoleMotionFallback(capabilities);

    // Always force an unsupported saved value back to OFF. This makes console
    // fallback disabled by default and prevents stale/manual settings from
    // enabling a path for which libnx does not yet expose mapped motion vectors.
    if (!consoleFallbackSupported && motion.allowConsoleMotionFallback) {
        motion.allowConsoleMotionFallback = false;
        artemis::input::SwitchMotionPolicyStore::instance().set(motion);
    }

    consoleMotionFallback->init("artemis/settings/console_motion_fallback"_i18n,
                                consoleFallbackSupported &&
                                    motion.allowConsoleMotionFallback,
                                [consoleFallbackSupported](bool enabled) {
        if (!consoleFallbackSupported)
            return;
        auto options = artemis::input::SwitchMotionPolicyStore::instance().get();
        options.allowConsoleMotionFallback = enabled;
        artemis::input::SwitchMotionPolicyStore::instance().set(options);
    });
    consoleMotionFallback->setEnabled(consoleFallbackSupported);
    if (!consoleFallbackSupported) {
        consoleMotionFallback->setDetailText(
            capabilities.libnxSevenSixAxisApiAvailable
                ? "artemis/settings/console_motion_api_unmapped"_i18n
                : "artemis/settings/console_motion_unavailable"_i18n);
    }
#else
    forwardMotion->init("artemis/settings/forward_motion"_i18n, true, [](bool) {});
    consoleMotionFallback->init("artemis/settings/console_motion_fallback"_i18n,
                                false, [](bool) {});
    forwardMotion->setEnabled(false);
    consoleMotionFallback->setEnabled(false);
#endif

#if ARTEMIS_HAS_ZOOM_PAN
    const auto zoom = artemis::video::ZoomPanStore::instance().get();
    rememberZoomPan->init("artemis/settings/remember_zoom_pan"_i18n,
                          zoom.rememberBetweenSessions,
                          [](bool enabled) {
        artemis::video::ZoomPanStore::instance().setRemember(enabled);
    });
    resetZoomPan->setText("artemis/settings/reset_zoom_pan"_i18n);
    resetZoomPan->registerClickAction([this](View*) {
        artemis::video::ZoomPanStore::instance().reset();
        resetZoomPan->setDetailText("artemis/settings/reset_zoom_pan_done"_i18n);
        return true;
    });
#else
    rememberZoomPan->init("artemis/settings/remember_zoom_pan"_i18n,
                          false, [](bool) {});
    rememberZoomPan->setEnabled(false);
    resetZoomPan->setText("artemis/settings/reset_zoom_pan"_i18n);
    resetZoomPan->setEnabled(false);
#endif

    refreshValues();
}

ArtemisSettingsTab::~ArtemisSettingsTab() {
    if (hasFrameRateSubscription)
        frameRate->getEvent()->unsubscribe(frameRateSubscription);
    Settings::instance().save();
    artemis::streaming::StreamProfileStore::instance().save();
}

View* ArtemisSettingsTab::create() { return new ArtemisSettingsTab(); }

void ArtemisSettingsTab::refreshFrameRateSelector() {
    const auto values = activeFrameRates();
    frameRate->setData(frameRateLabels(values));
    frameRate->setSelection(frameRateSelection(values, Settings::instance().fps()));

    if (hasFrameRateSubscription)
        frameRate->getEvent()->unsubscribe(frameRateSubscription);
    frameRateSubscription = frameRate->getEvent()->subscribe([this](int selected) {
        const auto currentValues = activeFrameRates();
        if (selected < 0 || selected >= static_cast<int>(currentValues.size()))
            return;
        Settings::instance().set_fps(currentValues[selected]);
        Settings::instance().save();
        refreshValues();
    });
    hasFrameRateSubscription = true;
}

void ArtemisSettingsTab::refreshValues() {
    const auto stored = artemis::streaming::StreamProfileStore::instance().get();
    width->setDetailText(std::to_string(stored.width));
    height->setDetailText(std::to_string(stored.height));
    exactBitrate->setDetailText(fmt::format("{:.1f} Mbps",
        static_cast<double>(Settings::instance().bitrate()) / 1000.0));

    const int configuredResolution = Settings::instance().resolution();
    const std::string resolutionText = stored.customResolutionEnabled
        ? fmt::format("{}x{}", stored.width, stored.height)
        : (configuredResolution == -1
               ? "settings/resolution_native"_i18n
               : fmt::format("{}x{}", configuredResolution * 16 / 9,
                             configuredResolution));

    activeProfile->setDetailText(fmt::format(
        "{} @ {} FPS, {}, {:.1f} Mbps, {} decoder threads",
        resolutionText,
        Settings::instance().fps(),
        getVideoCodecName(Settings::instance().video_codec()),
        static_cast<double>(Settings::instance().bitrate()) / 1000.0,
        Settings::instance().decoder_threads()));

    width->setEnabled(stored.customResolutionEnabled);
    height->setEnabled(stored.customResolutionEnabled);
}

void ArtemisSettingsTab::editWidth() {
    const auto current = artemis::streaming::StreamProfileStore::instance().get();
    Application::getImeManager()->openForNumber(
        [this, current](long number) {
            const int value = std::clamp(static_cast<int>(number), 640, 1920);
            artemis::streaming::StreamProfileStore::instance().setCustomResolution(
                current.customResolutionEnabled, value, current.height);
            refreshValues();
        },
        "artemis/settings/custom_width_title"_i18n,
        "artemis/settings/custom_width_hint"_i18n, 4,
        std::to_string(current.width), "", "", 0);
}

void ArtemisSettingsTab::editHeight() {
    const auto current = artemis::streaming::StreamProfileStore::instance().get();
    Application::getImeManager()->openForNumber(
        [this, current](long number) {
            const int value = std::clamp(static_cast<int>(number), 360, 1080);
            artemis::streaming::StreamProfileStore::instance().setCustomResolution(
                current.customResolutionEnabled, current.width, value);
            refreshValues();
        },
        "artemis/settings/custom_height_title"_i18n,
        "artemis/settings/custom_height_hint"_i18n, 4,
        std::to_string(current.height), "", "", 0);
}

void ArtemisSettingsTab::editBitrate() {
    const int currentMbps = std::max(1, Settings::instance().bitrate() / 1000);
    Application::getImeManager()->openForNumber(
        [this](long number) {
            const int mbps = std::clamp(static_cast<int>(number), 1, 100);
            Settings::instance().set_bitrate(mbps * 1000);
            Settings::instance().save();
            refreshValues();
        },
        "artemis/settings/exact_bitrate_title"_i18n,
        "artemis/settings/exact_bitrate_hint"_i18n, 3,
        std::to_string(currentMbps), "", "", 0);
}
