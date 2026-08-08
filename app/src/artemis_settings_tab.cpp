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
    customResolution->init("Use custom resolution", stored.customResolutionEnabled,
                           [this](bool enabled) {
        const auto value = artemis::streaming::StreamProfileStore::instance().get();
        artemis::streaming::StreamProfileStore::instance().setCustomResolution(
            enabled, value.width, value.height);
        refreshValues();
    });

    width->setText("Custom width");
    height->setText("Custom height");
    exactBitrate->setText("Exact bitrate");
    activeProfile->setText("Configured stream");

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

    frameRate->setText("Stream frame rate");
    refreshFrameRateSelector();

#if ARTEMIS_HAS_ADVANCED_STREAM
    const auto advanced = artemis::stream::AdvancedStreamOptionsStore::instance().get();
    unlockHighFps->init("Unlock 90 / 120 FPS", advanced.unlockAllFrameRates,
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
    forceFullRange->init("Force full-range video (Experimental)",
                         advanced.forceFullRangeVideo, [](bool enabled) {
        auto options = artemis::stream::AdvancedStreamOptionsStore::instance().get();
        options.forceFullRangeVideo = enabled;
        artemis::stream::AdvancedStreamOptionsStore::instance().set(options);
    });
    preventPacketLoss->init("Prevent packet loss (Experimental)",
                            advanced.preventPacketLoss, [](bool enabled) {
        auto options = artemis::stream::AdvancedStreamOptionsStore::instance().get();
        options.preventPacketLoss = enabled;
        artemis::stream::AdvancedStreamOptionsStore::instance().set(options);
    });
    preventPacketLoss->setDetailText("Preference only until Switch transport behavior is verified");
#else
    unlockHighFps->init("Unlock 90 / 120 FPS", false, [](bool) {});
    forceFullRange->init("Force full-range video (Experimental)", false, [](bool) {});
    preventPacketLoss->init("Prevent packet loss (Experimental)", false, [](bool) {});
    unlockHighFps->setEnabled(false);
    forceFullRange->setEnabled(false);
    preventPacketLoss->setEnabled(false);
    unlockHighFps->setDetailText("Advanced stream PR not present");
#endif

#if ARTEMIS_HAS_VIDEO_SCALE
    const auto currentScale = artemis::video::VideoScaleStore::instance().get();
    const int scaleSelection = currentScale == artemis::video::ScaleMode::Fit ? 0
                             : currentScale == artemis::video::ScaleMode::Fill ? 1 : 2;
    scaleMode->init("Video scale mode", {"Fit", "Fill", "Stretch"}, scaleSelection,
                    [](int selected) {
        const auto mode = selected == 0 ? artemis::video::ScaleMode::Fit
                        : selected == 2 ? artemis::video::ScaleMode::Stretch
                                        : artemis::video::ScaleMode::Fill;
        artemis::video::VideoScaleStore::instance().set(mode);
    });
#else
    scaleMode->init("Video scale mode", {"Fill"}, 0, [](int) {});
    scaleMode->setEnabled(false);
    scaleMode->setDetailText("Scaling PR not present");
#endif

#if ARTEMIS_HAS_MOTION_POLICY
    const auto motion = artemis::input::SwitchMotionPolicyStore::instance().get();
    forwardMotion->init("Forward Joy-Con / controller motion",
                        motion.allowGamepadMotionSensors, [](bool enabled) {
        auto options = artemis::input::SwitchMotionPolicyStore::instance().get();
        options.allowGamepadMotionSensors = enabled;
        artemis::input::SwitchMotionPolicyStore::instance().set(options);
    });
    consoleMotionFallback->init("Allow console motion fallback",
                                motion.allowConsoleMotionFallback, [](bool enabled) {
        auto options = artemis::input::SwitchMotionPolicyStore::instance().get();
        options.allowConsoleMotionFallback = enabled;
        artemis::input::SwitchMotionPolicyStore::instance().set(options);
    });
#else
    forwardMotion->init("Forward Joy-Con / controller motion", true, [](bool) {});
    consoleMotionFallback->init("Allow console motion fallback", false, [](bool) {});
    forwardMotion->setEnabled(false);
    consoleMotionFallback->setEnabled(false);
    forwardMotion->setDetailText("Motion policy PR not present");
#endif

#if ARTEMIS_HAS_ZOOM_PAN
    const auto zoom = artemis::video::ZoomPanStore::instance().get();
    rememberZoomPan->init("Remember Zoom & Pan", zoom.rememberBetweenSessions,
                          [](bool enabled) {
        artemis::video::ZoomPanStore::instance().setRemember(enabled);
    });
    resetZoomPan->setText("Reset Zoom & Pan");
    resetZoomPan->registerClickAction([this](View*) {
        artemis::video::ZoomPanStore::instance().reset();
        resetZoomPan->setDetailText("Reset to 1.0x / centered");
        return true;
    });
#else
    rememberZoomPan->init("Remember Zoom & Pan", false, [](bool) {});
    rememberZoomPan->setEnabled(false);
    resetZoomPan->setText("Reset Zoom & Pan");
    resetZoomPan->setEnabled(false);
    rememberZoomPan->setDetailText("Zoom/Pan PR not present");
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
               ? "Native"
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
        "Custom stream width", "640 - 1920", 4,
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
        "Custom stream height", "360 - 1080", 4,
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
        "Exact stream bitrate", "1 - 100 Mbps", 3,
        std::to_string(currentMbps), "", "", 0);
}
