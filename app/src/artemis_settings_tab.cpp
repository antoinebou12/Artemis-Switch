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

#if defined(__SWITCH__) && (defined(ENABLE_NETBIRD) || defined(ENABLE_WIREGUARD))
#include "remote_access/RemoteAccessManager.hpp"
#include "remote_access/RemoteAccessSelection.hpp"
#include "remote_access_provider_id.hpp"
#include "streaming/JsonFileBrowser.hpp"
#include "vpn/WireGuardManager.hpp"
#endif

#if defined(__SWITCH__) && (defined(ENABLE_NETBIRD) || defined(ENABLE_WIREGUARD))
#include <borealis/core/task.hpp>
#include <fstream>

namespace {

// Reads a NetBird setup key out of a file so it never has to be typed on the
// on-screen keyboard. Accepts either a bare key on its own line or a
// "setup_key = VALUE" / "key: VALUE" line, which is how netbird.conf-style
// files store it. Comment and blank lines are skipped.
std::string readSetupKeyFile(const std::string& path) {
    std::ifstream file(path);
    if (!file)
        return {};

    const auto trim = [](std::string text) {
        const auto begin = text.find_first_not_of(" \t\r\n\"'");
        if (begin == std::string::npos)
            return std::string{};
        const auto end = text.find_last_not_of(" \t\r\n\"'");
        return text.substr(begin, end - begin + 1);
    };

    std::string line;
    while (std::getline(file, line)) {
        line = trim(std::move(line));
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        const auto separator = line.find_first_of("=:");
        if (separator == std::string::npos)
            return line; // bare key on its own line

        std::string name = trim(line.substr(0, separator));
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) {
                           return static_cast<char>(std::tolower(c));
                       });
        if (name == "setup_key" || name == "setupkey" || name == "key")
            return trim(line.substr(separator + 1));
    }
    return {};
}

// Polls remote-access status while the settings tab is on screen. Connecting
// is asynchronous, so a value written once at construction would sit on
// "Connecting" forever.
class RemoteAccessStatusTask : public brls::RepeatingTask {
  public:
    explicit RemoteAccessStatusTask(std::function<void()> onTick)
        : brls::RepeatingTask(1000), onTick_(std::move(onTick)) {}

    void run() override {
        if (onTick_)
            onTick_();
    }

  private:
    std::function<void()> onTick_;
};

} // namespace
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
    // Provider choice is the single entry point. "Off" means no tunnel is
    // connected; both providers are always compiled in, so this is never a
    // statement about the build.
    const auto currentProvider = Settings::instance().remote_access_provider();
    remoteAccessProvider->init(
        "settings/remote_access_provider"_i18n,
        {"settings/remote_access_off"_i18n, "settings/wireguard"_i18n,
         "settings/netbird"_i18n},
        static_cast<int>(currentProvider),
        // The value callback only updates the cell's own text. Applying the
        // choice here would push the loading dialog while Dropdown is still
        // mid-selection: it fires this callback and THEN calls popActivity,
        // which would pop the dialog and strand the dropdown on screen.
        [](int) {},
        // Fires once the dropdown has finished popping, so it is safe to open
        // a dialog from here.
        [this](int selected) {
            const auto provider = providerFromSelectorIndex(selected);
            const auto previous = Settings::instance().remote_access_provider();
            if (provider == previous &&
                !RemoteAccessManager::instance().activeProviderId().empty()) {
                return;
            }

            if (provider == RemoteAccessProviderId::Off) {
                // Off is a real preference change, so record it and tear the
                // tunnel down.
                Settings::instance().set_remote_access_provider(
                    RemoteAccessProviderId::Off);
                Settings::instance().save();
                disconnectRemoteAccessAsync(
                    alive_, [this]() { refreshRemoteAccessRows(); });
                return;
            }

            applyRemoteAccessSelectionAsync(
                provider, alive_,
                [this](const RemoteAccessSelectionResult& result) {
                    // applyRemoteAccessSelection already leaves the provider
                    // selected on failure, so the rows for it stay visible and
                    // the configuration can be corrected and retried.
                    if (!result.started) {
                        brls::Application::notify(
                            result.status.empty()
                                ? "settings/remote_access_failed"_i18n
                                : brls::getStr(result.status));
                    }
                    refreshRemoteAccessRows();
                });
        });

    auto showConfigPath = [this](const std::string& path) {
        const std::string shown =
            path.empty() ? WireGuardManager::default_config_path() : path;
        wireguardConfigPath->setDetailText(
            shown.size() <= 40
                ? shown
                : shown.substr(0, 18) + "…" +
                      shown.substr(shown.size() - 18));
    };

    wireguardConfigPath->setText("settings/wireguard_config_path"_i18n);
    showConfigPath(Settings::instance().wireguard_config_path());
    wireguardConfigPath->registerClickAction([this, showConfigPath](View*) {
        // Browse for the file instead of retyping a long sdmc: path by hand.
        artemis::streaming::openFileBrowser(
            artemis::streaming::JsonFileBrowserMode::Import, {".conf"},
            "settings/wireguard_config_path_title"_i18n,
            [this, showConfigPath](const std::string& path) {
                if (path.empty())
                    return;
                Settings::instance().set_wireguard_config_path(path);
                Settings::instance().save();
                showConfigPath(path);
                // Re-apply immediately so a corrected file takes effect without
                // toggling the provider off and on again.
                if (Settings::instance().remote_access_provider() ==
                    RemoteAccessProviderId::WireGuard) {
                    applyRemoteAccessSelectionAsync(
                        RemoteAccessProviderId::WireGuard, alive_,
                        [this](const RemoteAccessSelectionResult&) {
                            refreshRemoteAccessRows();
                        });
                }
            });
        return true;
    });

    netbirdServer->setText("settings/netbird_server"_i18n);
    netbirdServer->setDetailText(Settings::instance().netbird_server());
    netbirdServer->registerClickAction([this](View*) {
        const std::string current = Settings::instance().netbird_server();
        Application::getPlatform()->getImeManager()->openForText(
            [this](const std::string& text) {
                if (text.empty())
                    return;
                Settings::instance().set_netbird_server(text);
                Settings::instance().save();
                netbirdServer->setDetailText(text);
            },
            "settings/netbird_server"_i18n, "", 120,
            current.empty() ? "https://api.netbird.io:443" : current, 0);
        return true;
    });

    netbirdSetupKey->setText("settings/netbird_setup_key"_i18n);
    netbirdSetupKey->registerClickAction([this](View*) {
        editNetBirdSetupKey();
        return true;
    });

    remoteAccessPreferLan->init(
        "settings/remote_access_prefer_lan"_i18n,
        Settings::instance().remote_access_prefer_lan(), [](bool enabled) {
            Settings::instance().set_remote_access_prefer_lan(enabled);
            Settings::instance().save();
        });

    // Explicit connect/disconnect. Without it the only way to retry a failed
    // tunnel was to reselect the provider.
    remoteAccessAction->registerClickAction([this](View*) {
        const auto provider = Settings::instance().remote_access_provider();
        if (provider == RemoteAccessProviderId::Off) {
            return true;
        }

        // Disconnect leaves the chosen provider alone: it is a transport
        // action, so reconnecting later must not need the setting again.
        if (!RemoteAccessManager::instance().activeProviderId().empty()) {
            disconnectRemoteAccessAsync(alive_,
                                        [this]() { refreshRemoteAccessRows(); });
            return true;
        }

        applyRemoteAccessSelectionAsync(
            provider, alive_,
            [this](const RemoteAccessSelectionResult& result) {
                if (!result.started) {
                    brls::Application::notify(
                        result.status.empty()
                            ? "settings/remote_access_failed"_i18n
                            : brls::getStr(result.status));
                }
                refreshRemoteAccessRows();
            });
        return true;
    });

    remoteAccessAutoConnect->init(
        "settings/remote_access_auto_connect"_i18n,
        Settings::instance().remote_access_auto_connect(), [](bool enabled) {
            Settings::instance().set_remote_access_auto_connect(enabled);
            Settings::instance().save();
        });

    wireguardStatus->setText("settings/wireguard_status"_i18n);
    remoteAccessAddress->setText("settings/remote_access_vpn_address"_i18n);
    remoteAccessPeers->setText("settings/remote_access_peers"_i18n);
    remoteAccessError->setText("settings/remote_access_last_error"_i18n);
    wireguardBackendStatus->setText(
        "settings/remote_access_backend_wireguard"_i18n);
    netbirdBackendStatus->setText(
        "settings/remote_access_backend_netbird"_i18n);

    // These rows are read-only readouts; keep them out of the focus order.
    for (brls::DetailCell* row :
         {static_cast<brls::DetailCell*>(remoteAccessAddress),
          static_cast<brls::DetailCell*>(remoteAccessPeers),
          static_cast<brls::DetailCell*>(remoteAccessError),
          static_cast<brls::DetailCell*>(wireguardBackendStatus),
          static_cast<brls::DetailCell*>(netbirdBackendStatus),
          static_cast<brls::DetailCell*>(wireguardStatus)}) {
        row->setFocusable(false);
    }

    refreshRemoteAccessRows();

    remoteAccessStatusTask_ =
        new RemoteAccessStatusTask([this]() { refreshRemoteAccessStatus(); });
    remoteAccessStatusTask_->start();
#else
    remoteAccessProvider->removeFromSuperView(true);
    wireguardConfigPath->removeFromSuperView(true);
    netbirdServer->removeFromSuperView(true);
    netbirdSetupKey->removeFromSuperView(true);
    remoteAccessPreferLan->removeFromSuperView(true);
    remoteAccessAutoConnect->removeFromSuperView(true);
    remoteAccessAction->removeFromSuperView(true);
    remoteAccessStatusHeader->removeFromSuperView(true);
    wireguardStatus->removeFromSuperView(true);
    remoteAccessAddress->removeFromSuperView(true);
    remoteAccessPeers->removeFromSuperView(true);
    remoteAccessError->removeFromSuperView(true);
    wireguardBackendStatus->removeFromSuperView(true);
    netbirdBackendStatus->removeFromSuperView(true);
#endif

    refreshValues();
}

ArtemisSettingsTab::~ArtemisSettingsTab() {
#if defined(__SWITCH__) && (defined(ENABLE_NETBIRD) || defined(ENABLE_WIREGUARD))
    // Any async continuation still in flight checks this before touching the
    // bound rows.
    alive_->store(false);
    // Stop before the bound rows go away; the task captures `this`.
    if (remoteAccessStatusTask_) {
        remoteAccessStatusTask_->stop();
        delete remoteAccessStatusTask_;
        remoteAccessStatusTask_ = nullptr;
    }
#endif
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

void ArtemisSettingsTab::refreshRemoteAccessRows() {
#if defined(__SWITCH__) && (defined(ENABLE_NETBIRD) || defined(ENABLE_WIREGUARD))
    const auto provider = Settings::instance().remote_access_provider();
    const auto visible = providerVisibility(provider);
    const bool active = provider != RemoteAccessProviderId::Off;

    const auto show = [](brls::View* view, bool shown) {
        view->setVisibility(shown ? brls::Visibility::VISIBLE
                                  : brls::Visibility::GONE);
    };

    // Keep the cell showing what is actually stored. Silent, so re-syncing
    // never re-fires the selection handler.
    remoteAccessProvider->setSelection(static_cast<int>(provider), true);

    show(wireguardConfigPath, visible.wireGuard);
    show(netbirdServer, visible.netBird);
    show(netbirdSetupKey, visible.netBird);

    // Prefer-LAN and connect-on-startup only mean something once a provider is
    // chosen.
    show(remoteAccessPreferLan, active);
    show(remoteAccessAutoConnect, active);
    show(remoteAccessAction, active);

    show(remoteAccessStatusHeader, active);
    show(wireguardStatus, active);
    show(remoteAccessAddress, active);
    show(remoteAccessPeers, active && visible.netBird);
#if defined(ENABLE_WIREGUARD)
    show(wireguardBackendStatus, active);
#else
    show(wireguardBackendStatus, false);
#endif
#if defined(ENABLE_NETBIRD)
    show(netbirdBackendStatus, active);
#else
    show(netbirdBackendStatus, false);
#endif

    if (visible.netBird) {
        netbirdSetupKey->setDetailText(
            Settings::instance().netbird_setup_key().empty()
                ? "settings/netbird_setup_key_unset"_i18n
                : "settings/netbird_setup_key_set"_i18n);
    }

    remoteAccessAction->setText(
        RemoteAccessManager::instance().activeProviderId().empty()
            ? "settings/remote_access_connect"_i18n
            : "settings/remote_access_disconnect"_i18n);

    refreshRemoteAccessStatus();
#endif
}

void ArtemisSettingsTab::refreshRemoteAccessStatus() {
#if defined(__SWITCH__) && (defined(ENABLE_NETBIRD) || defined(ENABLE_WIREGUARD))
    const auto provider = Settings::instance().remote_access_provider();
    if (provider == RemoteAccessProviderId::Off)
        return;

    auto& manager = RemoteAccessManager::instance();
    auto* active = manager.provider(remoteAccessProviderRuntimeId(provider));

    wireguardStatus->setDetailText(
        active ? brls::getStr(active->status())
               : "settings/remote_access_off"_i18n);

    const std::string address = active ? active->localAddress() : std::string{};
    remoteAccessAddress->setDetailText(
        address.empty() ? "settings/remote_access_unassigned"_i18n : address);

    if (active && provider == RemoteAccessProviderId::NetBird) {
        const auto peers = active->peers();
        // Report reachable separately from total: a mesh peer that is not
        // running Sunshine/Apollo is online but not streamable.
        const auto reachable = static_cast<size_t>(
            std::count_if(peers.begin(), peers.end(),
                          [](const RemoteAccessPeer& p) { return p.online; }));
        remoteAccessPeers->setDetailText(
            fmt::format("{} / {}", reachable, peers.size()));
    }

    const std::string error = active ? active->lastError() : std::string{};
    remoteAccessError->setVisibility(error.empty() ? brls::Visibility::GONE
                                                   : brls::Visibility::VISIBLE);
    if (!error.empty())
        remoteAccessError->setDetailText(brls::getStr(error));

    // Diagnostics row states which backend is actually linked, so a build that
    // cannot move packets can never look like a working one.
    wireguardBackendStatus->setDetailText(
        WireGuardManager::backend_is_real()
            ? "settings/remote_access_backend_isolated_real"_i18n
            : "settings/remote_access_backend_stub"_i18n);
#if defined(ENABLE_NETBIRD)
    netbirdBackendStatus->setDetailText(
        "settings/remote_access_backend_isolated_real"_i18n);
#endif
#endif
}

void ArtemisSettingsTab::editNetBirdSetupKey() {
#if defined(__SWITCH__) && (defined(ENABLE_NETBIRD) || defined(ENABLE_WIREGUARD))
    const bool hasKey = !Settings::instance().netbird_setup_key().empty();

    std::vector<std::string> options{
        "settings/netbird_setup_key_from_file"_i18n,
        "settings/netbird_setup_key_type"_i18n,
    };
    if (hasKey)
        options.push_back("settings/netbird_setup_key_clear"_i18n);

    const auto applyKey = [this](const std::string& key) {
        Settings::instance().set_netbird_setup_key(key);
        Settings::instance().save();
        refreshRemoteAccessRows();
        // Re-apply so a corrected key takes effect without reselecting the
        // provider.
        if (Settings::instance().remote_access_provider() ==
            RemoteAccessProviderId::NetBird) {
            applyRemoteAccessSelectionAsync(
                RemoteAccessProviderId::NetBird, alive_,
                [this](const RemoteAccessSelectionResult&) {
                    refreshRemoteAccessRows();
                });
        }
    };

    // Every branch below pushes another activity (file browser or IME), so the
    // work belongs in the dismiss callback: Dropdown fires its value callback
    // and only then calls popActivity, which would pop whatever we pushed.
    auto* dropdown = new brls::Dropdown(
        "settings/netbird_setup_key"_i18n, options, [](int) {}, 0,
        [this, applyKey, hasKey](int index) {
            if (index == 0) {
                artemis::streaming::openFileBrowser(
                    artemis::streaming::JsonFileBrowserMode::Import,
                    {".key", ".txt", ".conf"},
                    "settings/netbird_setup_key_from_file"_i18n,
                    [applyKey](const std::string& path) {
                        if (path.empty())
                            return;
                        const auto key = readSetupKeyFile(path);
                        if (key.empty()) {
                            brls::Application::notify(
                                "settings/netbird_setup_key_file_empty"_i18n);
                            return;
                        }
                        applyKey(key);
                    });
                return;
            }
            if (index == 1) {
                Application::getPlatform()->getImeManager()->openForText(
                    [applyKey](const std::string& text) {
                        if (!text.empty())
                            applyKey(text);
                    },
                    // Start empty rather than prefilled: never echo the stored
                    // key back onto the screen.
                    "settings/netbird_setup_key"_i18n, "", 120, "", 0);
                return;
            }
            if (hasKey && index == 2)
                applyKey("");
        });
    brls::Application::pushActivity(new brls::Activity(dropdown));
#endif
}
