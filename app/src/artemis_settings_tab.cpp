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

#include <algorithm>
#include <fmt/format.h>
#include <iomanip>
#include <sstream>
#include <vector>

using namespace brls;

ArtemisSettingsTab::ArtemisSettingsTab() {
    inflateFromXMLRes("xml/tabs/artemis_settings.xml");

    for (brls::DetailCell* row :
         {static_cast<brls::DetailCell*>(width),
          static_cast<brls::DetailCell*>(height),
          static_cast<brls::DetailCell*>(resetZoomPan),
          static_cast<brls::DetailCell*>(wireguardConfigPath),
          static_cast<brls::DetailCell*>(wireguardStatus)}) {
        row->title->setSingleLine(true);
        row->detail->setSingleLine(true);
    }

    const auto stored = artemis::streaming::StreamProfileStore::instance().get();
    customResolution->init("artemis/settings/use_custom_resolution"_i18n,
                           stored.customResolutionEnabled,
                           [this](bool enabled) {
        const auto value =
            artemis::streaming::StreamProfileStore::instance().get();
        artemis::streaming::StreamProfileStore::instance().setCustomResolution(
            enabled, value.width, value.height);
        refreshValues();
    });

    width->setText("artemis/settings/custom_width"_i18n);
    height->setText("artemis/settings/custom_height"_i18n);
    width->registerClickAction([this](View*) {
        editWidth();
        return true;
    });
    height->registerClickAction([this](View*) {
        editHeight();
        return true;
    });

#if ARTEMIS_HAS_ADVANCED_STREAM
    const auto advanced =
        artemis::stream::AdvancedStreamOptionsStore::instance().get();
    forceFullRange->init("artemis/settings/force_full_range"_i18n,
                         advanced.forceFullRangeVideo, [](bool enabled) {
        auto options =
            artemis::stream::AdvancedStreamOptionsStore::instance().get();
        options.forceFullRangeVideo = enabled;
        artemis::stream::AdvancedStreamOptionsStore::instance().set(options);
    });
    preventPacketLoss->init("artemis/settings/prevent_packet_loss"_i18n,
                            advanced.preventPacketLoss, [](bool enabled) {
        auto options =
            artemis::stream::AdvancedStreamOptionsStore::instance().get();
        options.preventPacketLoss = enabled;
        artemis::stream::AdvancedStreamOptionsStore::instance().set(options);
    });
    lowLatencyPacing->init("artemis/settings/low_latency_pacing"_i18n,
                           Settings::instance().low_latency_pacing(),
                           [](bool enabled) {
                               Settings::instance().set_low_latency_pacing(
                                   enabled);
                               Settings::instance().save();
                           });

    {
        int queueSize = Settings::instance().frames_queue_size();
        if (queueSize < 1)
            queueSize = 1;
        if (queueSize > 5)
            queueSize = 5;
        framesQueueSize->init(
            "artemis/settings/frames_queue_size"_i18n,
            {"1", "2", "3", "4", "5"}, queueSize - 1, [](int selected) {
                const int size = std::clamp(selected + 1, 1, 5);
                Settings::instance().set_frames_queue_size(size);
                Settings::instance().save();
            });
    }

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
            packetSelection = static_cast<int>(packetPresets.size());
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
                options.packetSize =
                    packetPresets[static_cast<size_t>(selected)];
                artemis::stream::AdvancedStreamOptionsStore::instance().set(
                    options);
                return;
            }
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
    forceFullRange->init("artemis/settings/force_full_range"_i18n, false,
                         [](bool) {});
    preventPacketLoss->init("artemis/settings/prevent_packet_loss"_i18n, false,
                            [](bool) {});
    lowLatencyPacing->init("artemis/settings/low_latency_pacing"_i18n,
                           Settings::instance().low_latency_pacing(),
                           [](bool enabled) {
                               Settings::instance().set_low_latency_pacing(
                                   enabled);
                               Settings::instance().save();
                           });
    framesQueueSize->init("artemis/settings/frames_queue_size"_i18n,
                          {"1", "2", "3", "4", "5"},
                          std::clamp(Settings::instance().frames_queue_size(),
                                     1, 5) -
                              1,
                          [](int selected) {
                              Settings::instance().set_frames_queue_size(
                                  std::clamp(selected + 1, 1, 5));
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
    const int scaleSelection =
        currentScale == artemis::video::ScaleMode::Fit      ? 0
        : currentScale == artemis::video::ScaleMode::Fill   ? 1
                                                            : 2;
    scaleMode->init("artemis/settings/video_scale_mode"_i18n,
                    {"artemis/settings/fit"_i18n, "artemis/settings/fill"_i18n,
                     "artemis/settings/stretch"_i18n},
                    scaleSelection, [](int selected) {
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
        auto options =
            artemis::input::SwitchMotionPolicyStore::instance().get();
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

    consoleMotionFallback->init(
        "artemis/settings/console_motion_fallback"_i18n,
        consoleFallbackSupported && motion.allowConsoleMotionFallback,
        [consoleFallbackSupported](bool enabled) {
            if (!consoleFallbackSupported)
                return;
            auto options =
                artemis::input::SwitchMotionPolicyStore::instance().get();
            options.allowConsoleMotionFallback = enabled;
            artemis::input::SwitchMotionPolicyStore::instance().set(options);
        });
    consoleMotionFallback->setEnabled(consoleFallbackSupported);
#else
    forwardMotion->init("artemis/settings/forward_motion"_i18n, true,
                        [](bool) {});
    consoleMotionFallback->init("artemis/settings/console_motion_fallback"_i18n,
                                false, [](bool) {});
    forwardMotion->setEnabled(false);
    consoleMotionFallback->setEnabled(false);
#endif

    {
        float mouseProgress =
            static_cast<float>(Settings::instance().get_mouse_speed_multiplier()) /
            100.0f;
        mouseSpeedSlider->getProgressEvent()->subscribe([this](float value) {
            Settings::instance().set_mouse_speed_multiplier(int(value * 100));
            std::stringstream stream;
            stream << std::fixed << std::setprecision(1)
                   << Settings::instance().mouse_speed_scale();
            mouseSpeedHeader->setSubtitle(stream.str() + "x");
        });
        mouseSpeedSlider->setProgress(mouseProgress);
        std::stringstream stream;
        stream << std::fixed << std::setprecision(1)
               << Settings::instance().mouse_speed_scale();
        mouseSpeedHeader->setSubtitle(stream.str() + "x");
    }

#if ARTEMIS_HAS_ZOOM_PAN
    const auto zoom = artemis::video::ZoomPanStore::instance().get();
    rememberZoomPan->init("artemis/settings/remember_zoom_pan"_i18n,
                          zoom.rememberBetweenSessions, [](bool enabled) {
        artemis::video::ZoomPanStore::instance().setRemember(enabled);
    });

    auto refreshZoomPanSliders = [this] {
        const auto state = artemis::video::normalizeZoomPan(
            artemis::video::ZoomPanStore::instance().get().state);
        zoomHeader->setSubtitle(fmt::format("{:.1f}x", state.zoom));
        panXHeader->setSubtitle(fmt::format("{:.2f}", state.panX));
        panYHeader->setSubtitle(fmt::format("{:.2f}", state.panY));
        zoomSlider->setProgress(artemis::video::zoomToSlider(state.zoom));
        panXSlider->setProgress(artemis::video::panToSlider(state.panX));
        panYSlider->setProgress(artemis::video::panToSlider(state.panY));
    };
    refreshZoomPanSliders();

    zoomSlider->getProgressEvent()->subscribe([this](float progress) {
        auto& store = artemis::video::ZoomPanStore::instance();
        auto state = store.get().state;
        state.zoom = artemis::video::sliderToZoom(progress);
        store.setState(state);
        zoomHeader->setSubtitle(fmt::format(
            "{:.1f}x", artemis::video::normalizeZoomPan(store.get().state).zoom));
    });
    panXSlider->getProgressEvent()->subscribe([this](float progress) {
        auto& store = artemis::video::ZoomPanStore::instance();
        auto state = store.get().state;
        state.panX = artemis::video::sliderToPan(progress);
        store.setState(state);
        panXHeader->setSubtitle(fmt::format(
            "{:.2f}", artemis::video::normalizeZoomPan(store.get().state).panX));
    });
    panYSlider->getProgressEvent()->subscribe([this](float progress) {
        auto& store = artemis::video::ZoomPanStore::instance();
        auto state = store.get().state;
        state.panY = artemis::video::sliderToPan(progress);
        store.setState(state);
        panYHeader->setSubtitle(fmt::format(
            "{:.2f}", artemis::video::normalizeZoomPan(store.get().state).panY));
    });

    resetZoomPan->setText("artemis/settings/reset_zoom_pan"_i18n);
    resetZoomPan->registerClickAction([this, refreshZoomPanSliders](View*) {
        artemis::video::ZoomPanStore::instance().reset();
        refreshZoomPanSliders();
        resetZoomPan->setDetailText("artemis/settings/reset_zoom_pan_done"_i18n);
        return true;
    });
#else
    rememberZoomPan->init("artemis/settings/remember_zoom_pan"_i18n, false,
                          [](bool) {});
    rememberZoomPan->setEnabled(false);
    resetZoomPan->setText("artemis/settings/reset_zoom_pan"_i18n);
    resetZoomPan->setFocusable(false);
    zoomSlider->setFocusable(false);
    panXSlider->setFocusable(false);
    panYSlider->setFocusable(false);
#endif

#if defined(__SWITCH__) && (defined(ENABLE_NETBIRD) || defined(ENABLE_WIREGUARD))
    // Only persist "enabled" once the tunnel actually came up. Previously the
    // setting was written first, so a failed enable left the switch on and the
    // feature enabled across restarts while nothing was tunnelled.
    auto applyWireguardEnabled = [this](bool value) {
        bool effective = false;
        if (value) {
            effective = WireGuardManager::instance().enable_from_settings();
        } else {
            WireGuardManager::instance().disable();
        }
        Settings::instance().set_wireguard_enabled(effective);
        Settings::instance().save();
        wireguardStatus->setDetailText(
            WireGuardManager::instance().status_text());
        if (value && !effective) {
            // Put the switch back where the truth is.
            wireguardEnabled->setOn(false, true);
        }
    };

    wireguardEnabled->init("settings/wireguard_enabled"_i18n,
                           Settings::instance().wireguard_enabled(),
                           applyWireguardEnabled);

    auto showConfigPath = [this](const std::string& path) {
        const std::string shown =
            path.empty() ? WireGuardManager::default_config_path() : path;
        wireguardConfigPath->setDetailText(
            shown.size() <= 40
                ? shown
                : shown.substr(0, 18) + "…" + shown.substr(shown.size() - 18));
    };

    wireguardConfigPath->setText("settings/wireguard_config_path"_i18n);
    showConfigPath(Settings::instance().wireguard_config_path());
    wireguardConfigPath->registerClickAction([this, showConfigPath](View*) {
        const std::string current =
            Settings::instance().wireguard_config_path();
        Application::getPlatform()->getImeManager()->openForText(
            [this, showConfigPath](const std::string& text) {
                Settings::instance().set_wireguard_config_path(text);
                Settings::instance().save();
                showConfigPath(text);
                if (Settings::instance().wireguard_enabled()) {
                    WireGuardManager::instance().enable_from_settings();
                    wireguardStatus->setDetailText(
                        WireGuardManager::instance().status_text());
                }
            },
            "settings/wireguard_config_path_title"_i18n, "", 120,
            current.empty() ? WireGuardManager::default_config_path() : current,
            0);
        return true;
    });
    wireguardStatus->setText("settings/wireguard_status"_i18n);
    {
        // Diagnostics: never let a stub build look like a working tunnel.
        std::string detail = WireGuardManager::instance().status_text();
        if (!WireGuardManager::backend_is_real()) {
            detail += " — " + "settings/remote_access_backend_stub"_i18n;
        }
        wireguardStatus->setDetailText(detail);
    }
#else
    wireguardEnabled->removeFromSuperView(true);
    wireguardConfigPath->removeFromSuperView(true);
    wireguardStatus->removeFromSuperView(true);
#endif

    refreshValues();
}

ArtemisSettingsTab::~ArtemisSettingsTab() {
    Settings::instance().save();
    artemis::streaming::StreamProfileStore::instance().save();
}

View* ArtemisSettingsTab::create() { return new ArtemisSettingsTab(); }

void ArtemisSettingsTab::refreshValues() {
    const auto stored = artemis::streaming::StreamProfileStore::instance().get();
    width->setDetailText(std::to_string(stored.width));
    height->setDetailText(std::to_string(stored.height));
    width->setFocusable(stored.customResolutionEnabled);
    height->setFocusable(stored.customResolutionEnabled);
}

void ArtemisSettingsTab::editWidth() {
    const auto current =
        artemis::streaming::StreamProfileStore::instance().get();
    Application::getImeManager()->openForNumber(
        [this, current](long number) {
            const int value = std::clamp(static_cast<int>(number), 640, 2560);
            artemis::streaming::StreamProfileStore::instance()
                .setCustomResolution(current.customResolutionEnabled, value,
                                     current.height);
            refreshValues();
        },
        "artemis/settings/custom_width_title"_i18n,
        "artemis/settings/custom_width_hint"_i18n, 4,
        std::to_string(current.width), "", "", 0);
}

void ArtemisSettingsTab::editHeight() {
    const auto current =
        artemis::streaming::StreamProfileStore::instance().get();
    Application::getImeManager()->openForNumber(
        [this, current](long number) {
            const int value = std::clamp(static_cast<int>(number), 360, 1440);
            artemis::streaming::StreamProfileStore::instance()
                .setCustomResolution(current.customResolutionEnabled,
                                     current.width, value);
            refreshValues();
        },
        "artemis/settings/custom_height_title"_i18n,
        "artemis/settings/custom_height_hint"_i18n, 4,
        std::to_string(current.height), "", "", 0);
}
