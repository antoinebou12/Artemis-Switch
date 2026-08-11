//
//  ingame_overlay.hpp
//  Moonlight
//
//  Created by Даниил Vinogradov on 29.05.2021.
//

#pragma once

#include "streaming_view.hpp"
#include "views/boolean_slider_cell.hpp"
#include <borealis.hpp>

// MARK: - Ingame Overlay View
class IngameOverlay : public brls::Box {
  public:
    explicit IngameOverlay(StreamingView* streamView);
    ~IngameOverlay() override = default;

    brls::AppletFrame* getAppletFrame() override;
    bool isTranslucent() override { return true; }

  private:
    StreamingView* streamView;

    BRLS_BIND(brls::Box, backplate, "backplate");
    BRLS_BIND(brls::AppletFrame, applet, "applet");
};

// MARK: - Logout Tab
class LogoutTab : public brls::Box {
  public:
    explicit LogoutTab(StreamingView* streamView);

  private:
    StreamingView* streamView;

    BRLS_BIND(brls::DetailCell, disconnect, "disconnect");
    BRLS_BIND(brls::DetailCell, terminateButton, "terminate");
};

// MARK: - Quick Tab
class QuickTab : public brls::Box {
  public:
    explicit QuickTab(StreamingView* streamView);
    ~QuickTab() override = default;

  private:
    StreamingView* streamView;

    BRLS_BIND(brls::DetailCell, quickKeyboard, "quick_keyboard");
    BRLS_BIND(brls::DetailCell, quickMoveLeft, "quick_move_left");
    BRLS_BIND(brls::DetailCell, quickMoveRight, "quick_move_right");
    BRLS_BIND(brls::DetailCell, quickTouch, "quick_touch");
    BRLS_BIND(brls::DetailCell, quickHostShortcuts, "quick_host_shortcuts");
    BRLS_BIND(brls::DetailCell, quickServerCommands, "quick_server_commands");
    BRLS_BIND(brls::DetailCell, quickMouse, "quick_mouse");
    BRLS_BIND(brls::Header, volumeHeader, "volume_header");
    BRLS_BIND(brls::Slider, volumeSlider, "volume_slider");
};

// MARK: - Options Tab
class OptionsTab : public brls::Box {
  public:
    explicit OptionsTab(StreamingView* streamView);
    ~OptionsTab() override;

  private:
    StreamingView* streamView;
    std::string clipboardText;

    static std::string getTextFromButtons(std::vector<brls::ControllerButton> buttons);
    static NVGcolor getColorFromButtons(const std::vector<brls::ControllerButton>& buttons);
    void setupButtonsSelectorCell(brls::DetailCell* cell, const std::vector<brls::ControllerButton>& buttons);
    void openClipboardPanel();
    void fetchClipboard();
    void uploadClipboard(bool pasteAfterUpload);

    BRLS_BIND(brls::DetailCell, inputOverlayButton, "input_overlay");
    BRLS_BIND(brls::SelectorCell, keyboardType, "keyboard_type");
    BRLS_BIND(brls::SelectorCell, keyboardFingers, "keyboard_fingers");
    BRLS_BIND(brls::BooleanCell, touchscreenMouseMode, "touchscreen_mouse_mode");
    BRLS_BIND(brls::DetailCell, optionsPointerMode, "options_pointer_mode");
    BRLS_BIND(brls::DetailCell, optionsControllers, "options_controllers");
    BRLS_BIND(brls::DetailCell, optionsDiagnostics, "options_diagnostics");
    BRLS_BIND(brls::DetailCell, optionsClipboard, "options_clipboard");
    BRLS_BIND(brls::DetailCell, optionsRotation, "options_rotation");
    BRLS_BIND(brls::DetailCell, guideKeyButtons, "guide_key_buttons");
    BRLS_BIND(brls::SelectorCell, guideBySystemButton, "guide_by_system_button");
    BRLS_BIND(brls::Header, rumbleForceHeader, "rumble_slider_header");
    BRLS_BIND(brls::Slider, rumbleForceSlider, "rumble_slider");
    BRLS_BIND(brls::BooleanCell, swapStickToDpad, "swap_stick_to_dpad");
    BRLS_BIND(brls::Header, mouseHeader, "mouse_speed_header");
    BRLS_BIND(brls::Slider, mouseSlider, "mouse_speed_slider");
    BRLS_BIND(brls::Header, imageAdjustmentsHeader, "image_adjustments_header");
    BRLS_BIND(BooleanSliderCell, ditheringButton, "dithering");
    BRLS_BIND(BooleanSliderCell, rcasButton, "rcas");
    BRLS_BIND(brls::BooleanCell, upscalingButton, "upscaling");
    BRLS_BIND(brls::SelectorCell, upscalingModeButton, "upscaling_mode");
};
