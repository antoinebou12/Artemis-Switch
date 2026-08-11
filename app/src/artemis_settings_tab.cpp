#include "artemis_settings_tab.hpp"

#include "Settings.hpp"
#include "helper.hpp"
#include "streaming/StreamProfileStore.hpp"

#if __has_include("features/stream/AdvancedStreamOptionsStore.hpp")
#include "features/stream/AdvancedStreamOptionsStore.hpp"
#define ARTEMIS_HAS_ADVANCED_STREAM 1
#else
#define ARTEMIS_HAS_ADVANCED_STREAM 0
#endif

#if __has_include("features/stream/FrameRateOptions.hpp")
#include "features/stream/FrameRateOptions.hpp"
#define ARTEMIS_HAS_FRAME_RATE_PRESETS 1
#else
#define ARTEMIS_HAS_FRAME_RATE_PRESETS 0
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

#if defined(__SWITCH__) && defined(ENABLE_WIREGUARD)
#include "vpn/WireGuardManager.hpp"
#endif

#if __has_include("features/apollo/ApolloHostOptions.hpp")
#include "features/apollo/ApolloHostOptions.hpp"
#include "features/apollo/ApolloHostOptionsStore.hpp"
#define ARTEMIS_HAS_APOLLO_HOST_OPTIONS 1
#else
#define ARTEMIS_HAS_APOLLO_HOST_OPTIONS 0
#endif

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fmt/format.h>
#include <vector>

using namespace brls;

namespace {
#if ARTEMIS_HAS_FRAME_RATE_PRESETS
std::vector<std::string> activeFrameRateLabels() {
    std::vector<std::string> labels;
    for (const auto& preset : artemis::stream::availableFrameRatePresets())
        labels.push_back(artemis::stream::frameRatePresetLabel(preset));
    return labels;
}
#else
std::vector<int> activeFrameRates() {
#if ARTEMIS_HAS_ADVANCED_STREAM
    return artemis::stream::availableFrameRates(
        artemis::stream::AdvancedStreamOptionsStore::instance().get());
#else
    return {30, 40, 60, 90, 120};
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
#endif

int resolutionPresetIndex(int width, int height) {
    if (width == 1280 && height == 720)
        return 1; // Handheld 16:9
    if (width == 1920 && height == 1080)
        return 2; // Docked 16:9
    if (width == 960 && height == 720)
        return 3; // Handheld 4:3
    if (width == 1440 && height == 1080)
        return 4; // Docked 4:3
    return 0; // Custom
}

void applyResolutionPreset(int selected) {
    auto stored = artemis::streaming::StreamProfileStore::instance().get();
    int width = stored.width;
    int height = stored.height;
    if (selected == 1) {
        width = 1280;
        height = 720;
    } else if (selected == 2) {
        width = 1920;
        height = 1080;
    } else if (selected == 3) {
        width = 960;
        height = 720;
    } else if (selected == 4) {
        width = 1440;
        height = 1080;
    }
    artemis::streaming::StreamProfileStore::instance().setCustomResolution(
        true, width, height);

    if (selected == 1 || selected == 2) {
        Settings::instance().set_aspect_ratio(
            artemis::streaming::StreamAspectRatio::Ratio16x9);
        Settings::instance().save();
    } else if (selected == 3 || selected == 4) {
        Settings::instance().set_aspect_ratio(
            artemis::streaming::StreamAspectRatio::Ratio4x3);
        Settings::instance().save();
    }

#if ARTEMIS_HAS_APOLLO_HOST_OPTIONS
    // Persist preferred Apollo virtual-display target for the next connection.
    auto options = artemis::apollo::ApolloHostOptions{};
    if (selected == 1 || selected == 3)
        options.target = artemis::apollo::VirtualDisplayTarget::Handheld;
    else if (selected == 2 || selected == 4)
        options.target = artemis::apollo::VirtualDisplayTarget::Docked;
    else
        options.target = artemis::apollo::VirtualDisplayTarget::Custom;
    options.customWidth = width;
    options.customHeight = height;
    options.refreshRate = Settings::instance().fps();
    // Use a shared default key until a host is selected for streaming.
    artemis::apollo::ApolloHostOptionsStore::instance().set("default", options);
#endif
}
}

ArtemisSettingsTab::ArtemisSettingsTab() {
    inflateFromXMLRes("xml/tabs/artemis_settings.xml");

    const std::array<DetailCell*, 13> compactRows = {
        customResolution, width, height, exactBitrate,
        frameRate, forceFullRange, preventPacketLoss, packetSize,
        scaleMode, rememberZoomPan, resetZoomPan, forwardMotion,
        consoleMotionFallback};
    for (auto* row : compactRows) {
        row->title->setSingleLine(true);
        row->detail->setSingleLine(true);
    }

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

    resolutionPreset->init(
        "artemis/settings/resolution_preset"_i18n,
        {"artemis/settings/preset_custom"_i18n,
         "artemis/settings/preset_handheld"_i18n,
         "artemis/settings/preset_docked"_i18n,
         "artemis/settings/preset_handheld_4_3"_i18n,
         "artemis/settings/preset_docked_4_3"_i18n},
        resolutionPresetIndex(stored.width, stored.height),
        [this](int selected) {
            applyResolutionPreset(selected);
            customResolution->setOn(true);
            refreshValues();
        });

#if ARTEMIS_HAS_ADVANCED_STREAM
    const auto advanced = artemis::stream::AdvancedStreamOptionsStore::instance().get();
    forceFullRange->init("artemis/settings/force_full_range"_i18n,
                         advanced.forceFullRangeVideo, [](bool enabled) {
        auto options = artemis::stream::AdvancedStreamOptionsStore::instance().get();
        options.forceFullRangeVideo = enabled;
        artemis::stream::AdvancedStreamOptionsStore::instance().set(options);
    });
    preventPacketLoss->init("artemis/settings/prevent_packet_loss"_i18n,
                            advanced.preventPacketLoss, [](bool enabled) {
        auto options = artemis::stream::AdvancedStreamOptionsStore::instance().get();
        options.preventPacketLoss = enabled;
        artemis::stream::AdvancedStreamOptionsStore::instance().set(options);
    });
    lowLatencyPacing->init("artemis/settings/low_latency_pacing"_i18n,
                           Settings::instance().low_latency_pacing(),
                           [](bool enabled) {
                               Settings::instance().set_low_latency_pacing(enabled);
                               Settings::instance().save();
                           });

    const auto packetPresets = std::vector<int>{
        artemis::stream::kPacketSizeAuto, 1024, 1346,
        artemis::stream::kPacketSizeDefault};
    int packetSelection = 0;
    for (size_t i = 0; i < packetPresets.size(); ++i) {
        if (packetPresets[i] == advanced.packetSize) {
            packetSelection = static_cast<int>(i);
            break;
        }
        if (i + 1 == packetPresets.size() && advanced.packetSize > 0)
            packetSelection = static_cast<int>(packetPresets.size()); // Custom
    }
    packetSize->init(
        "artemis/settings/packet_size"_i18n,
        {"artemis/settings/packet_size_auto"_i18n, "1024", "1346", "1392",
         "artemis/settings/packet_size_custom"_i18n},
        packetSelection,
        [packetPresets](int selected) {
            auto options =
                artemis::stream::AdvancedStreamOptionsStore::instance().get();
            if (selected >= 0 &&
                selected < static_cast<int>(packetPresets.size())) {
                options.packetSize = packetPresets[static_cast<size_t>(selected)];
                artemis::stream::AdvancedStreamOptionsStore::instance().set(
                    options);
                return;
            }
            // Custom: ask for an explicit byte size.
            const int current =
                options.packetSize > 0
                    ? options.packetSize
                    : artemis::stream::kPacketSizeDefault;
            Application::getImeManager()->openForNumber(
                [](long number) {
                    auto opts = artemis::stream::AdvancedStreamOptionsStore::
                        instance()
                            .get();
                    opts.packetSize = artemis::stream::clampPacketSize(
                        static_cast<int>(number));
                    artemis::stream::AdvancedStreamOptionsStore::instance().set(
                        opts);
                },
                "artemis/settings/packet_size"_i18n,
                "artemis/settings/packet_size_hint"_i18n, 5,
                std::to_string(current), "", "", 0);
        });
#else
    forceFullRange->init("artemis/settings/force_full_range"_i18n, false, [](bool) {});
    preventPacketLoss->init("artemis/settings/prevent_packet_loss"_i18n, false, [](bool) {});
    lowLatencyPacing->init("artemis/settings/low_latency_pacing"_i18n,
                           Settings::instance().low_latency_pacing(),
                           [](bool enabled) {
                               Settings::instance().set_low_latency_pacing(enabled);
                               Settings::instance().save();
                           });
    packetSize->init("artemis/settings/packet_size"_i18n,
                     {"artemis/settings/packet_size_auto"_i18n}, 0, [](int) {});
    forceFullRange->setEnabled(false);
    preventPacketLoss->setEnabled(false);
    packetSize->setEnabled(false);
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
    resetZoomPan->setFocusable(false);
#endif

#if defined(__SWITCH__) && defined(ENABLE_WIREGUARD)
    wireguardEnabled->init(
        "settings/wireguard_enabled"_i18n,
        Settings::instance().wireguard_enabled(), [this](bool value) {
            Settings::instance().set_wireguard_enabled(value);
            if (value) {
                WireGuardManager::instance().enable_from_settings();
            } else {
                WireGuardManager::instance().disable();
            }
            wireguardStatus->setDetailText(
                WireGuardManager::instance().status_text());
        });
    wireguardConfigPath->setText("settings/wireguard_config_path"_i18n);
    {
        const std::string path = Settings::instance().wireguard_config_path();
        const std::string fallback = "sdmc:/switch/Artemis-Switch/wg0.conf";
        const std::string shown = path.empty() ? fallback : path;
        wireguardConfigPath->setDetailText(
            shown.size() <= 40
                ? shown
                : shown.substr(0, 18) + "…" + shown.substr(shown.size() - 18));
    }
    wireguardConfigPath->registerClickAction([this](View*) {
        const std::string current =
            Settings::instance().wireguard_config_path();
        Application::getPlatform()->getImeManager()->openForText(
            [this](const std::string& text) {
                Settings::instance().set_wireguard_config_path(text);
                wireguardConfigPath->setDetailText(
                    text.size() <= 40
                        ? text
                        : text.substr(0, 18) + "…" +
                              text.substr(text.size() - 18));
                if (Settings::instance().wireguard_enabled()) {
                    WireGuardManager::instance().enable_from_settings();
                    wireguardStatus->setDetailText(
                        WireGuardManager::instance().status_text());
                }
            },
            "settings/wireguard_config_path_title"_i18n, "", 120,
            current.empty() ? "sdmc:/switch/Artemis-Switch/wg0.conf" : current,
            0);
        return true;
    });
    wireguardStatus->setText("settings/wireguard_status"_i18n);
    wireguardStatus->setDetailText(WireGuardManager::instance().status_text());
#else
    wireguardEnabled->removeFromSuperView(true);
    wireguardConfigPath->removeFromSuperView(true);
    wireguardStatus->removeFromSuperView(true);
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
#if ARTEMIS_HAS_FRAME_RATE_PRESETS
    const auto presets = artemis::stream::availableFrameRatePresets();
    frameRate->setData(activeFrameRateLabels());
    frameRate->setSelection(artemis::stream::frameRatePresetIndex(
        Settings::instance().fps(),
        Settings::instance().client_refresh_rate_x100()));

    if (hasFrameRateSubscription)
        frameRate->getEvent()->unsubscribe(frameRateSubscription);
    frameRateSubscription = frameRate->getEvent()->subscribe([this](int selected) {
        const auto current = artemis::stream::availableFrameRatePresets();
        if (selected < 0 || selected >= static_cast<int>(current.size()))
            return;
        const auto& preset = current[static_cast<size_t>(selected)];
        Settings::instance().set_fps(preset.fps);
        Settings::instance().set_client_refresh_rate_x100(
            preset.clientRefreshRateX100);
        Settings::instance().save();
        refreshValues();
    });
    hasFrameRateSubscription = true;
#else
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
#endif
}

void ArtemisSettingsTab::refreshValues() {
    const auto stored = artemis::streaming::StreamProfileStore::instance().get();
    width->setDetailText(std::to_string(stored.width));
    height->setDetailText(std::to_string(stored.height));
    exactBitrate->setDetailText(fmt::format("{:.1f} Mbps",
        static_cast<double>(Settings::instance().bitrate()) / 1000.0));
    resolutionPreset->setSelection(
        resolutionPresetIndex(stored.width, stored.height), true);

    width->setFocusable(stored.customResolutionEnabled);
    height->setFocusable(stored.customResolutionEnabled);
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
