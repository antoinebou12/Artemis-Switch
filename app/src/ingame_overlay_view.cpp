//
//  ingame_overlay.cpp
//  Moonlight
//
//  Created by XITRIX on 29.05.2021.
//

#ifdef PLATFORM_SWITCH
#include <borealis/platforms/switch/switch_input.hpp>
#endif

#include "helper.hpp"
#include "ingame_overlay_view.hpp"
#include "streaming_input_overlay.hpp"
#include "button_selecting_dialog.hpp"
#include "UpscalingSupport.hpp"
#include "features/video/UpscalingModeSelect.hpp"
#include "video/VideoScaleStore.hpp"
#include "features/input/ControllerTopology.hpp"
#include "features/input/ControllerDiagnostics.hpp"
#include "features/input/InputSettingsStore.hpp"
#include "features/input/HostKeyboardShortcuts.hpp"
#include "features/apollo/ApolloHostOptionsStore.hpp"
#include "features/video/DisplayTransformStore.hpp"
#include "streaming/InputManager.hpp"
#include "Limelight.h"
#include "libgamestream/client.h"

#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <cstdint>

#define SET_SETTING(n, func)                                                   \
    case n:                                                                    \
        Settings::instance().func;                                             \
        break;

#define GET_SETTINGS(combo_box, n, i)                                          \
    case n:                                                                    \
        combo_box->setSelection(i);                                            \
        break;

#define DEFAULT                                                                \
    default:                                                                   \
        break;

using namespace brls;

namespace {
void updateStrengthControl(BooleanSliderCell* cell,
                           bool enabled, const std::string& detailText) {
    cell->setSliderVisibility(enabled ? brls::Visibility::VISIBLE
                                      : brls::Visibility::GONE);
    cell->setValueText(enabled ? detailText : "");
}

float sliderProgressToDitheringStrength(float progress) {
    return static_cast<float>(int(progress * 9.0f + 0.5f) + 1);
}

float ditheringStrengthToSliderProgress(float strength) {
    if (strength < 1.0f)
        strength = 1.0f;
    else if (strength > 10.0f)
        strength = 10.0f;

    return (strength - 1.0f) / 9.0f;
}

std::string getDitheringStrengthText(float strength) {
    return std::to_string(int(strength));
}

std::string getRcasStrengthText(float strength) {
    return std::to_string(int(strength * 100.0f)) + "%";
}

std::string scaleModeText(artemis::video::ScaleMode mode) {
    switch (mode) {
    case artemis::video::ScaleMode::Fit:
        return "artemis/settings/fit"_i18n;
    case artemis::video::ScaleMode::Stretch:
        return "artemis/settings/stretch"_i18n;
    case artemis::video::ScaleMode::Fill:
    default:
        return "artemis/settings/fill"_i18n;
    }
}

std::string virtualDisplayTargetText(
    artemis::apollo::VirtualDisplayTarget target) {
    using Target = artemis::apollo::VirtualDisplayTarget;
    switch (target) {
    case Target::Off: return "artemis/overlay/vd_off"_i18n;
    case Target::CurrentProfile: return "artemis/overlay/vd_current"_i18n;
    case Target::Handheld: return "artemis/overlay/vd_handheld"_i18n;
    case Target::Docked: return "artemis/overlay/vd_docked"_i18n;
    case Target::PortraitHandheld:
        return "artemis/overlay/vd_portrait_handheld"_i18n;
    case Target::PortraitDocked:
        return "artemis/overlay/vd_portrait_docked"_i18n;
    case Target::Custom: return "artemis/overlay/vd_custom"_i18n;
    }
    return "artemis/overlay/vd_off"_i18n;
}

std::string rotationText(artemis::video::Rotation rotation) {
    return std::to_string(static_cast<int>(rotation)) + "°";
}

class ControllerDiagnosticsPanel final : public brls::Box {
public:
    ControllerDiagnosticsPanel() : brls::Box(brls::Axis::COLUMN) {
        setPadding(18, 24, 18, 24);
        setGrow(1);

        live = new brls::Label();
        live->setHorizontalAlign(brls::HorizontalAlign::LEFT);
        live->setFontSize(20);
        live->setGrow(1);
        addView(live);

        auto* low = new brls::DetailCell();
        low->setText("artemis/overlay/rumble_test_low"_i18n);
        low->setDetailText("250 ms");
        low->registerClickAction([this](brls::View*) {
            pulseRumble(selectedSlot, 32767, 0);
            return true;
        });
        addView(low);

        auto* high = new brls::DetailCell();
        high->setText("artemis/overlay/rumble_test_high"_i18n);
        high->setDetailText("250 ms");
        high->registerClickAction([this](brls::View*) {
            pulseRumble(selectedSlot, 0, 32767);
            return true;
        });
        addView(high);

        auto* both = new brls::DetailCell();
        both->setText("artemis/overlay/rumble_test_both"_i18n);
        both->setDetailText("artemis/overlay/rumble_test_both_hint"_i18n);
        both->registerClickAction([this](brls::View*) {
            pulseRumble(selectedSlot, 32767, 32767);
            return true;
        });
        addView(both);

        auto* allPads = new brls::DetailCell();
        allPads->setText("artemis/overlay/rumble_test_all"_i18n);
        allPads->setDetailText("artemis/overlay/rumble_test_all_hint"_i18n);
        allPads->registerClickAction([this](brls::View*) {
            const int count = std::max(
                1, artemis::input::clampControllerCount(
                       brls::Application::getPlatform()
                           ->getInputManager()
                           ->getControllersConnectedCount()));
            auto* input = brls::Application::getPlatform()->getInputManager();
            const auto nowMs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
            for (int slot = 0; slot < count; ++slot) {
                input->sendRumble(slot, 32767, 32767);
                artemis::input::ControllerDiagnostics::instance().recordRumble(
                    slot, 32767, 32767, nowMs);
            }
            brls::delay(250, [count] {
                auto* inputManager =
                    brls::Application::getPlatform()->getInputManager();
                for (int slot = 0; slot < count; ++slot)
                    inputManager->sendRumble(slot, 0, 0);
            });
            return true;
        });
        addView(allPads);

        auto* nextSlot = new brls::DetailCell();
        nextSlot->setText("artemis/overlay/controller_slot"_i18n);
        nextSlot->setDetailText("P1");
        nextSlot->registerClickAction([this, nextSlot](brls::View*) {
            const int count = std::max(
                1, artemis::input::clampControllerCount(
                       brls::Application::getPlatform()
                           ->getInputManager()
                           ->getControllersConnectedCount()));
            selectedSlot = (selectedSlot + 1) % count;
            nextSlot->setDetailText("P" + std::to_string(selectedSlot + 1));
            refresh();
            return true;
        });
        addView(nextSlot);
        slotButton = nextSlot;
        refresh();
    }

    ~ControllerDiagnosticsPanel() override {
        const int count = std::max(
            1, artemis::input::clampControllerCount(
                   brls::Application::getPlatform()
                       ->getInputManager()
                       ->getControllersConnectedCount()));
        auto* input = brls::Application::getPlatform()->getInputManager();
        for (int slot = 0; slot < count; ++slot)
            input->sendRumble(slot, 0, 0);
    }

    void frame(brls::FrameContext* ctx) override {
        // Stream input is paused while the overlay has focus; sample pads here.
        MoonlightInputManager::instance().sampleDiagnostics();
        refresh();
        brls::Box::frame(ctx);
    }

private:
    void pulseRumble(int slot, unsigned short low, unsigned short high) {
        auto* input = brls::Application::getPlatform()->getInputManager();
        input->sendRumble(slot, low, high);
        artemis::input::ControllerDiagnostics::instance().recordRumble(
            slot, low, high,
            static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count()));
        brls::delay(250, [slot] {
            brls::Application::getPlatform()->getInputManager()->sendRumble(
                slot, 0, 0);
        });
    }

    void refresh() {
        const auto value =
            artemis::input::ControllerDiagnostics::instance().snapshot(
                selectedSlot);
        if (slotButton)
            slotButton->setDetailText("P" + std::to_string(selectedSlot + 1));
        std::ostringstream out;
        out << "artemis/overlay/controller_slot"_i18n << ": P"
            << value.slot + 1 << " · " << value.identity << '\n';
        out << "artemis/overlay/buttons"_i18n << ": 0x" << std::uppercase
            << std::hex << std::setw(4) << std::setfill('0') << value.buttons
            << std::dec << "   LT " << static_cast<int>(value.leftTrigger)
            << "   RT " << static_cast<int>(value.rightTrigger) << '\n';
        out << "LS: " << value.leftStickX << ", " << value.leftStickY
            << "   RS: " << value.rightStickX << ", " << value.rightStickY
            << '\n';
        out << std::fixed << std::setprecision(2)
            << "Accel: " << value.acceleration.x << ", "
            << value.acceleration.y << ", " << value.acceleration.z << '\n'
            << "Gyro: " << value.gyroscope.x << ", " << value.gyroscope.y
            << ", " << value.gyroscope.z << '\n';
        out << "artemis/overlay/sample_rate"_i18n << ": "
            << std::setprecision(1) << value.motionSampleRateHz << " Hz   "
            << "artemis/overlay/dropped_samples"_i18n << ": "
            << value.droppedMotionSamples << '\n';
        out << "artemis/overlay/motion_source"_i18n << ": "
            << (value.handheldMotionFallback
                    ? "artemis/overlay/handheld_fallback"_i18n
                    : "artemis/overlay/controller_motion"_i18n)
            << '\n';
        out << "artemis/overlay/last_rumble"_i18n << ": "
            << value.lastRumbleLow << " / " << value.lastRumbleHigh;
        live->setText(out.str());
    }

    brls::Label* live = nullptr;
    brls::DetailCell* slotButton = nullptr;
    int selectedSlot = 0;
};

}

bool debug = false;

// MARK: - Ingame Overlay View
IngameOverlay::IngameOverlay(StreamingView* streamView)
    : streamView(streamView) {
    brls::Application::registerXMLView(
        "LogoutTab", [streamView]() { return new LogoutTab(streamView); });
    brls::Application::registerXMLView(
        "QuickTab", [streamView]() { return new QuickTab(streamView); });
    brls::Application::registerXMLView(
        "OptionsTab", [streamView]() { return new OptionsTab(streamView); });

    this->inflateFromXMLRes("xml/views/ingame_overlay/overlay.xml");

    addGestureRecognizer(
        new TapGestureRecognizer([this](TapGestureStatus status, Sound* sound) {
            if (status.state == GestureState::END)
                this->dismiss();
        }));

    applet->addGestureRecognizer(new TapGestureRecognizer(
        [](TapGestureStatus status, Sound* sound) {}));

    getAppletFrameItem()->title =
        streamView->getHost().hostname + ": " + streamView->getApp().name;
    updateAppletFrameItem();
}

brls::AppletFrame* IngameOverlay::getAppletFrame() { return applet; }

// MARK: - Logout Tab
LogoutTab::LogoutTab(StreamingView* streamView) : streamView(streamView) {
    this->inflateFromXMLRes("xml/views/ingame_overlay/logout_tab.xml");

    disconnect->setText("streaming/disconnect"_i18n);
    disconnect->registerClickAction([this, streamView](View* view) {
        this->dismiss([streamView] {
            streamView->terminate(
                Settings::instance().terminate_app_on_disconnect());
        });
        return true;
    });

    terminateButton->setText("streaming/terminate"_i18n);
    terminateButton->registerClickAction([this, streamView](View* view) {
        this->dismiss([streamView] { streamView->terminate(true); });
        return true;
    });
}

// MARK: - Quick Tab
QuickTab::QuickTab(StreamingView* streamView) : streamView(streamView) {
    this->inflateFromXMLRes("xml/views/ingame_overlay/quick_tab.xml");

    const std::array<DetailCell*, 6> quickRows = {
        quickKeyboard, quickMoveLeft, quickMoveRight, quickHostShortcuts,
        quickServerCommands, quickMouse};
    for (auto* row : quickRows) {
        row->title->setSingleLine(true);
        row->detail->setSingleLine(true);
    }

    quickKeyboard->setText("artemis/overlay/show_keyboard"_i18n);
    quickKeyboard->registerClickAction([this, streamView](View*) {
        this->dismiss([streamView] { streamView->showKeyboard(); });
        return true;
    });

    const auto wireMoveWindow = [](DetailCell* cell, bool moveRight) {
        cell->setText(moveRight ? "artemis/overlay/move_window_right"_i18n
                                : "artemis/overlay/move_window_left"_i18n);
        cell->setDetailText(moveRight
                                ? "artemis/overlay/move_window_right_hint"_i18n
                                : "artemis/overlay/move_window_left_hint"_i18n);
        cell->registerClickAction([cell, moveRight](View*) {
            const bool sent =
                MoonlightInputManager::moveActiveWindowToDisplay(moveRight);
            cell->setDetailText(sent ? "artemis/overlay/sent"_i18n
                                     : "artemis/overlay/send_failed"_i18n);
            return true;
        });
    };
    wireMoveWindow(quickMoveLeft, false);
    wireMoveWindow(quickMoveRight, true);

    quickHostShortcuts->setText("artemis/overlay/host_shortcuts"_i18n);
    quickHostShortcuts->setDetailText(
        "artemis/overlay/host_shortcuts_hint"_i18n);
    quickHostShortcuts->registerClickAction([this](View*) {
        const auto& presets = artemis::input::standardShortcuts();
        std::vector<std::string> names;
        std::vector<size_t> indexes;
        names.reserve(presets.size());
        indexes.reserve(presets.size());
        for (size_t i = 0; i < presets.size(); ++i) {
            // Left/Right move have dedicated Quick rows.
            if (presets[i].id == "move_window_left" ||
                presets[i].id == "move_window_right")
                continue;
            names.push_back(presets[i].name);
            indexes.push_back(i);
        }
        auto* dropdown = new Dropdown(
            "artemis/overlay/host_shortcuts"_i18n, names,
            [this, indexes](int selected) {
                if (selected < 0 ||
                    static_cast<size_t>(selected) >= indexes.size())
                    return;
                const auto& presets = artemis::input::standardShortcuts();
                const bool sent = MoonlightInputManager::sendKeyboardShortcut(
                    presets[indexes[selected]].keys);
                quickHostShortcuts->setDetailText(
                    sent ? "artemis/overlay/sent"_i18n
                         : "artemis/overlay/send_failed"_i18n);
            },
            0);
        Application::pushActivity(new Activity(dropdown));
        return true;
    });

    const auto server =
        GameStreamClient::instance().server_data(streamView->getHost());
    constexpr uint32_t serverCommandPermission = 0x00100000;
    const bool commandsAllowed =
        server.isApollo() &&
        (!server.hasApolloPermissionField ||
         (server.permission & serverCommandPermission) != 0) &&
        !server.serverCommands.empty();

    quickServerCommands->setText("artemis/overlay/server_commands"_i18n);
    quickServerCommands->setDetailText(
        commandsAllowed ? "artemis/overlay/server_commands_hint"_i18n
                        : "artemis/overlay/apollo_only"_i18n);
    quickServerCommands->registerClickAction([this](View*) {
        const auto host =
            GameStreamClient::instance().server_data(this->streamView->getHost());
        constexpr uint32_t permission = 0x00100000;
        const bool allowed =
            host.isApollo() &&
            (!host.hasApolloPermissionField ||
             (host.permission & permission) != 0) &&
            !host.serverCommands.empty();
        if (!allowed) {
            auto* dialog = new Dialog("artemis/overlay/apollo_only"_i18n);
            dialog->addButton("common/close"_i18n, [] {});
            dialog->open();
            return true;
        }

        std::vector<std::string> names;
        names.reserve(host.serverCommands.size());
        for (size_t i = 0; i < host.serverCommands.size() && i <= UINT8_MAX; ++i)
            names.push_back(host.serverCommands[i]);

        auto* dropdown = new Dropdown(
            "artemis/overlay/server_commands"_i18n, names,
            [this](int selected) {
                if (selected < 0 || selected > UINT8_MAX)
                    return;
                const bool sent =
                    LiSendExecServerCmd(static_cast<uint8_t>(selected)) == 0;
                quickServerCommands->setDetailText(
                    sent ? "artemis/overlay/sent"_i18n
                         : "artemis/overlay/send_failed"_i18n);
            },
            0);
        Application::pushActivity(new Activity(dropdown));
        return true;
    });

    quickMouse->setText("artemis/overlay/mouse_controls"_i18n);
    quickMouse->registerClickAction([this](View*) {
        this->dismiss([this]() {
            auto* overlay = new StreamingInputOverlay(this->streamView);
            Application::pushActivity(new Activity(overlay));
        });
        return true;
    });
}

// MARK: - Options Tab
OptionsTab::OptionsTab(StreamingView* streamView) : streamView(streamView) {
    this->inflateFromXMLRes("xml/views/ingame_overlay/options_tab.xml");

    const auto connectedControllerCount = [] {
        const int reported = Application::getPlatform()
                                 ->getInputManager()
                                 ->getControllersConnectedCount();
        return artemis::input::clampControllerCount(reported);
    };
    const auto controllerCountText = [connectedControllerCount] {
        return std::to_string(connectedControllerCount()) + " / " +
               std::to_string(artemis::input::MaxSupportedControllers);
    };

    auto& inputStore = artemis::input::InputSettingsStore::instance();
    optionsPointerMode->setText("artemis/overlay/pointer_mode"_i18n);
    optionsPointerMode->setDetailText(
        artemis::input::pointerModeName(inputStore.pointer().mode));
    optionsPointerMode->registerClickAction([this](View*) {
        auto& store = artemis::input::InputSettingsStore::instance();
        auto settings = store.pointer();
        constexpr std::array modes = {
            artemis::input::PointerMode::MultiTouch,
            artemis::input::PointerMode::Absolute,
            artemis::input::PointerMode::AbsoluteSwapped,
            artemis::input::PointerMode::TrackpadNatural,
            artemis::input::PointerMode::TrackpadGaming,
            artemis::input::PointerMode::Disabled,
        };
        const auto current = std::find(modes.begin(), modes.end(), settings.mode);
        settings.mode = current == modes.end() || std::next(current) == modes.end()
                            ? modes.front()
                            : *std::next(current);
        MoonlightInputManager::instance().dropInput();
        store.setPointer(settings);
        optionsPointerMode->setDetailText(
            artemis::input::pointerModeName(settings.mode));
        return true;
    });

    optionsControllers->setText("artemis/overlay/controllers"_i18n);
    optionsControllers->setDetailText(controllerCountText());
    optionsControllers->registerClickAction(
        [this, connectedControllerCount, controllerCountText](View*) {
            const auto players = artemis::input::connectedControllerPlayers(
                connectedControllerCount());
            std::string message =
                "artemis/overlay/connected_controllers"_i18n;
            message += "\n\n";
            if (players.empty()) {
                message += "artemis/overlay/no_controllers"_i18n;
            } else {
                for (std::size_t i = 0; i < players.size(); ++i) {
                    if (i != 0)
                        message += "\n";
                    message += "P" + std::to_string(players[i]) + " → " +
                               "artemis/overlay/pc_slot"_i18n + " " +
                               std::to_string(players[i]) + " · " +
                               "artemis/overlay/rumble_motion"_i18n;
                }
            }

            optionsControllers->setDetailText(controllerCountText());
            auto* dialog = new Dialog(message);
            dialog->addButton("common/close"_i18n, [] {});
            dialog->open();
            return true;
        });

    optionsDiagnostics->setText("artemis/overlay/controller_diagnostics"_i18n);
    optionsDiagnostics->setDetailText("artemis/overlay/live_input_debug"_i18n);
    optionsDiagnostics->registerClickAction([](View*) {
        auto* dialog = new Dialog(new ControllerDiagnosticsPanel());
        dialog->addButton("common/close"_i18n, [] {});
        dialog->open();
        return true;
    });

    const auto server = GameStreamClient::instance().server_data(streamView->getHost());
    optionsClipboard->setText("artemis/overlay/clipboard"_i18n);
    optionsClipboard->setDetailText(server.isApollo()
                                        ? "artemis/overlay/fetch_edit_paste"_i18n
                                        : "artemis/overlay/apollo_only"_i18n);
    optionsClipboard->registerClickAction([this](View*) {
        openClipboardPanel();
        return true;
    });

    auto& transformStore = artemis::video::DisplayTransformStore::instance();
    optionsRotation->setText("artemis/overlay/rotation"_i18n);
    optionsRotation->setDetailText(rotationText(transformStore.get().rotation));
    optionsRotation->registerClickAction([this](View*) {
        auto& store = artemis::video::DisplayTransformStore::instance();
        const auto rotation = artemis::video::nextRotation(store.get().rotation);
        MoonlightInputManager::instance().dropInput();
        store.setRotation(rotation);
        optionsRotation->setDetailText(rotationText(rotation));
        return true;
    });

    guideKeyButtons->setText("settings/guide_key_buttons"_i18n);
    setupButtonsSelectorCell(guideKeyButtons,
                             Settings::instance().guide_key_options().buttons);
    guideKeyButtons->registerClickAction([this](View* view) {
        ButtonSelectingDialog* dialog = ButtonSelectingDialog::create(
            "settings/guide_key_setup_message"_i18n, [this](auto buttons) {
                auto options = Settings::instance().guide_key_options();
                options.buttons = buttons;
                Settings::instance().set_guide_key_options(options);
                setupButtonsSelectorCell(guideKeyButtons, buttons);
            });

        dialog->open();
        return true;
    });

#ifndef PLATFORM_SWITCH
    guideBySystemButton->removeFromSuperView();
#else
    guideBySystemButton->init(
        "settings/use_system_button"_i18n,
        {"hints/off"_i18n, "settings/buttons/screenshot"_i18n, "settings/buttons/home"_i18n},
        (int) Settings::instance().get_guide_system_button(), [this](int value) {
            if (value != 0 && Settings::instance().get_overlay_system_button() == (ButtonOverrideType) value) {
                brls::sync([this, value](){
                    showError("settings/system_button_duplication_error"_i18n, [](){});
                });
                guideBySystemButton->setSelection((int) Settings::instance().get_guide_system_button(), true);
                return;
            }

            Settings::instance().set_guide_system_button((ButtonOverrideType) value);

            auto color = Settings::instance().get_guide_system_button() == ButtonOverrideType::NONE ?
                Application::getTheme()["brls/text_disabled"] : Application::getTheme()["brls/accent"];
            guideBySystemButton->setDetailTextColor(color);
        });
    auto color = Settings::instance().get_guide_system_button() == ButtonOverrideType::NONE ?
         Application::getTheme()["brls/text_disabled"] : Application::getTheme()["brls/accent"];
    guideBySystemButton->setDetailTextColor(color);
#endif

    volumeHeader->setSubtitle(
        std::to_string(Settings::instance().get_volume()) + "%");
    float amplification =
        Settings::instance().get_volume_amplification() ? 500.0f : 100.0f;
    float progress = (float) Settings::instance().get_volume() / amplification;
    volumeSlider->getProgressEvent()->subscribe(
        [this, amplification](float progress) {
            int volume = int(progress * amplification);
            Settings::instance().set_volume(volume);
            volumeHeader->setSubtitle(std::to_string(volume) + "%");
        });
    volumeSlider->setProgress(progress);

    float rumbleForceProgress = Settings::instance().get_rumble_force();
    rumbleForceSlider->getProgressEvent()->subscribe([this](float value) {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(1) << int(value * 100);
        rumbleForceHeader->setSubtitle(stream.str() + "%");
        Settings::instance().set_rumble_force(value);
    });
    rumbleForceSlider->setProgress(rumbleForceProgress);

    float mouseProgress =
        ((float) Settings::instance().get_mouse_speed_multiplier() / 100.0f);
    mouseSlider->getProgressEvent()->subscribe([this](float value) {
        Settings::instance().set_mouse_speed_multiplier(int(value * 100));
        std::stringstream stream;
        stream << std::fixed << std::setprecision(1)
               << Settings::instance().mouse_speed_scale();
        mouseHeader->setSubtitle(stream.str() + "x");
    });
    mouseSlider->setProgress(mouseProgress);
    {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(1)
               << Settings::instance().mouse_speed_scale();
        mouseHeader->setSubtitle(stream.str() + "x");
    }

    inputOverlayButton->setText("streaming/mouse_input"_i18n);
    inputOverlayButton->registerClickAction([this](View* view) {
        this->dismiss([this]() {
            auto* overlay = new StreamingInputOverlay(this->streamView);
            Application::pushActivity(new Activity(overlay));
        });
        return true;
    });

    std::vector<std::string> keyboardTypes = {
        "settings/keyboard_compact"_i18n,
        "settings/keyboard_fullsized"_i18n,
        "settings/keyboard_numpad"_i18n};
    keyboardType->setText("settings/keyboard_type"_i18n);
    keyboardType->setData(keyboardTypes);
    switch (Settings::instance().get_keyboard_type()) {
        GET_SETTINGS(keyboardType, COMPACT, 0)
        GET_SETTINGS(keyboardType, FULLSIZED, 1)
        GET_SETTINGS(keyboardType, NUMPAD, 2)
        DEFAULT
    }
    keyboardType->getEvent()->subscribe([](int selected) {
        switch (selected) {
            SET_SETTING(0, set_keyboard_type(COMPACT))
            SET_SETTING(1, set_keyboard_type(FULLSIZED))
            SET_SETTING(2, set_keyboard_type(NUMPAD))
            DEFAULT
        }
    });

    std::vector<std::string> keyboardFingersOptions = {
        "3", "4", "5", "hints/off"_i18n};
    keyboardFingers->setText("settings/keyboard_fingers"_i18n);
    keyboardFingers->setData(keyboardFingersOptions);
    switch (Settings::instance().get_keyboard_fingers()) {
        GET_SETTINGS(keyboardFingers, 3, 0)
        GET_SETTINGS(keyboardFingers, 4, 1)
        GET_SETTINGS(keyboardFingers, 5, 2)
        GET_SETTINGS(keyboardFingers, -1, 3)
        DEFAULT
    }
    keyboardFingers->getEvent()->subscribe([](int selected) {
        switch (selected) {
            SET_SETTING(0, set_keyboard_fingers(3))
            SET_SETTING(1, set_keyboard_fingers(4))
            SET_SETTING(2, set_keyboard_fingers(5))
            SET_SETTING(3, set_keyboard_fingers(-1))
            DEFAULT
        }
    });

    touchscreenMouseMode->init("settings/touchscreen_mouse_mode"_i18n,
                               Settings::instance().touchscreen_mouse_mode(),
                               [](bool value) {
                                   Settings::instance().set_touchscreen_mouse_mode(value);
                                   auto pointer = artemis::input::InputSettingsStore::instance().pointer();
                                   pointer.mode = artemis::input::pointerModeFromLegacyTouchscreen(value);
                                   artemis::input::InputSettingsStore::instance().setPointer(pointer);
                               });

    swapStickToDpad->init("settings/swap_stick_to_dpad"_i18n, Settings::instance().swap_joycon_stick_to_dpad(),
                          [](bool value) { Settings::instance().set_swap_joycon_stick_to_dpad(value); });

    onscreenLogButton->init("streaming/show_logs"_i18n,
                            Settings::instance().write_log(), [](bool value) {
                                Settings::instance().set_write_log(value);
                                brls::Application::enableDebuggingView(value);
                            });

    debugButton->init(
        "streaming/debug_info"_i18n, streamView->draw_stats,
        [streamView](bool value) { streamView->draw_stats = value; });

#ifdef SUPPORT_UPSCALING
    if (!isVideoUpscalingSupported()) {
        imageAdjustmentsHeader->removeFromSuperView(true);
        ditheringButton->removeFromSuperView(true);
        upscalingButton->removeFromSuperView(true);
        upscalingModeButton->removeFromSuperView(true);
        rcasButton->removeFromSuperView(true);
    } else {
        auto updateDitheringControls = [this](bool enabled) {
            updateStrengthControl(ditheringButton, enabled,
                                  getDitheringStrengthText(
                                      Settings::instance().dithering_strength()));
        };
        auto updateRcasControls = [this](bool enabled) {
            updateStrengthControl(rcasButton, enabled,
                                  getRcasStrengthText(
                                      Settings::instance().rcas_strength()));
        };

        ditheringButton->init(
            "settings/dithering"_i18n, Settings::instance().dithering(),
            [updateDitheringControls](bool value) {
                Settings::instance().set_dithering(value);
                updateDitheringControls(value);
            });

        const float ditheringStrength = Settings::instance().dithering_strength();
        ditheringButton->getProgressEvent()->subscribe([this](float value) {
            const float strength = sliderProgressToDitheringStrength(value);
            Settings::instance().set_dithering_strength(strength);
            updateStrengthControl(ditheringButton,
                                  Settings::instance().dithering(),
                                  getDitheringStrengthText(strength));
        });
        ditheringButton->setProgress(
            ditheringStrengthToSliderProgress(ditheringStrength));
        updateDitheringControls(Settings::instance().dithering());

upscalingButton->removeFromSuperView(true);
#if defined(PLATFORM_APPLE) && !defined(PLATFORM_TVOS)
        constexpr bool kMetalFxChoices = true;
        upscalingModeButton->init(
            "settings/upscaling"_i18n,
            {"hints/off"_i18n, "MetalFX", "FSR1"},
            artemis::video::upscaling_selector_index(
                (int)Settings::instance().upscaling_mode(), kMetalFxChoices),
            [](int value) {
                Settings::instance().set_upscaling_mode((UpscalingMode)
                    artemis::video::upscaling_mode_from_selector(value, true));
            });
#else
        constexpr bool kMetalFxChoices = false;
        upscalingModeButton->init(
            "settings/upscaling"_i18n,
            {"hints/off"_i18n, "FSR1"},
            artemis::video::upscaling_selector_index(
                (int)Settings::instance().upscaling_mode(), kMetalFxChoices),
            [](int value) {
                Settings::instance().set_upscaling_mode((UpscalingMode)
                    artemis::video::upscaling_mode_from_selector(value, false));
            });
#endif
        rcasButton->init(
            "settings/rcas_sharpening"_i18n, Settings::instance().rcas(),
            [updateRcasControls](bool value) {
                Settings::instance().set_rcas(value);
                updateRcasControls(value);
            });

        const float rcasStrength = Settings::instance().rcas_strength();
        rcasButton->getProgressEvent()->subscribe([this](float value) {
            Settings::instance().set_rcas_strength(value);
            updateStrengthControl(rcasButton,
                                  Settings::instance().rcas(),
                                  getRcasStrengthText(value));
        });
        rcasButton->setProgress(rcasStrength);
        updateRcasControls(Settings::instance().rcas());
    }
#else
    imageAdjustmentsHeader->removeFromSuperView(true);
    ditheringButton->removeFromSuperView(true);
    upscalingButton->removeFromSuperView(true);
    upscalingModeButton->removeFromSuperView(true);
    rcasButton->removeFromSuperView(true);
#endif
}

void OptionsTab::openClipboardPanel() {
    const auto server = GameStreamClient::instance().server_data(streamView->getHost());
    if (!server.isApollo()) {
        auto* dialog = new Dialog("artemis/overlay/apollo_only"_i18n);
        dialog->addButton("common/close"_i18n, [] {});
        dialog->open();
        return;
    }

    constexpr uint32_t clipboardSet = 0x00010000;
    constexpr uint32_t clipboardRead = 0x00020000;
    const bool canRead = !server.hasApolloPermissionField ||
                         (server.permission & clipboardRead) != 0;
    const bool canSet = !server.hasApolloPermissionField ||
                        (server.permission & clipboardSet) != 0;
    std::string message = "artemis/overlay/clipboard_panel"_i18n;
    message += "\n\n";
    message += clipboardText.empty()
                   ? "artemis/overlay/clipboard_empty"_i18n
                   : std::to_string(clipboardText.size()) + " bytes";
    auto* dialog = new Dialog(message);
    if (canRead)
        dialog->addButton("artemis/overlay/clipboard_fetch"_i18n,
                          [this] { fetchClipboard(); });
    dialog->addButton("artemis/overlay/clipboard_edit"_i18n, [this] {
        Application::getImeManager()->openForText(
            [this](std::string text) {
                if (text.size() <= 64 * 1024) {
                    clipboardText = std::move(text);
                    optionsClipboard->setDetailText(
                        std::to_string(clipboardText.size()) + " bytes");
                }
            }, "artemis/overlay/clipboard_edit"_i18n,
            "64 KiB plain text", 64 * 1024, clipboardText);
    });
    if (canSet) {
        dialog->addButton("artemis/overlay/clipboard_upload"_i18n,
                          [this] { uploadClipboard(false); });
        dialog->addButton("artemis/overlay/clipboard_paste"_i18n,
                          [this] { uploadClipboard(true); });
    }
    dialog->addButton("common/close"_i18n, [] {});
    dialog->open();
}

void OptionsTab::fetchClipboard() {
    auto server = GameStreamClient::instance().server_data(streamView->getHost());
    ASYNC_RETAIN
    brls::async([ASYNC_TOKEN, server = std::move(server)]() mutable {
        std::string text;
        const int result = gs_clipboard_get(&server, &text);
        const std::string error = result == GS_OK ? std::string{} : gs_error();
        brls::sync([ASYNC_TOKEN, result, text = std::move(text), error]() mutable {
            ASYNC_RELEASE
            if (result == GS_OK) {
                clipboardText = std::move(text);
                optionsClipboard->setDetailText(
                    std::to_string(clipboardText.size()) + " bytes");
            } else {
                optionsClipboard->setDetailText(error);
            }
        });
    });
}

void OptionsTab::uploadClipboard(bool pasteAfterUpload) {
    auto server = GameStreamClient::instance().server_data(streamView->getHost());
    const std::string text = clipboardText;
    ASYNC_RETAIN
    brls::async([ASYNC_TOKEN, server = std::move(server), text,
                 pasteAfterUpload]() mutable {
        const int result = gs_clipboard_set(&server, text);
        const std::string error = result == GS_OK ? std::string{} : gs_error();
        brls::sync([ASYNC_TOKEN, result, error, pasteAfterUpload] {
            ASYNC_RELEASE
            if (result == GS_OK) {
                if (pasteAfterUpload)
                    MoonlightInputManager::sendKeyboardShortcut({0xA2, 0x56});
                optionsClipboard->setDetailText(
                    "artemis/overlay/clipboard_uploaded"_i18n);
            } else {
                optionsClipboard->setDetailText(error);
            }
        });
    });
}

OptionsTab::~OptionsTab() { Settings::instance().save(); }

std::string
OptionsTab::getTextFromButtons(std::vector<ControllerButton> buttons) {
    std::string buttonsText;
    if (!buttons.empty()) {
        for (int i = 0; i < buttons.size(); i++) {
            buttonsText += brls::Hint::getKeyIcon(buttons[i], true);
            if (i < buttons.size() - 1)
                buttonsText += " + ";
        }
    } else {
        buttonsText = "hints/off"_i18n;
    }
    return buttonsText;
}

NVGcolor
OptionsTab::getColorFromButtons(const std::vector<brls::ControllerButton>& buttons) {
    Theme theme = Application::getTheme();
    return buttons.empty() ? theme["brls/text_disabled"]
                           : theme["brls/list/listItem_value_color"];
}

void OptionsTab::setupButtonsSelectorCell(brls::DetailCell* cell, const std::vector<ControllerButton>& buttons) {
    cell->setDetailText(getTextFromButtons(buttons));
    cell->setDetailTextColor(getColorFromButtons(buttons));
}
