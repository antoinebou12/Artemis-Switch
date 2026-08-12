#pragma once

#include <borealis.hpp>

class ArtemisSettingsTab : public brls::Box {
public:
    ArtemisSettingsTab();
    ~ArtemisSettingsTab() override;

    static brls::View* create();

private:
    void refreshValues();
    void editWidth();
    void editHeight();

    BRLS_BIND(brls::BooleanCell, customResolution, "custom_resolution");
    BRLS_BIND(brls::DetailCell, width, "custom_width");
    BRLS_BIND(brls::DetailCell, height, "custom_height");
    BRLS_BIND(brls::BooleanCell, forceFullRange, "force_full_range");
    BRLS_BIND(brls::BooleanCell, preventPacketLoss, "prevent_packet_loss");
    BRLS_BIND(brls::BooleanCell, lowLatencyPacing, "low_latency_pacing");
    BRLS_BIND(brls::SelectorCell, framesQueueSize, "frames_queue_size");
    BRLS_BIND(brls::SelectorCell, packetSize, "packet_size");

    BRLS_BIND(brls::SelectorCell, scaleMode, "scale_mode");
    BRLS_BIND(brls::Header, mouseSpeedHeader, "mouse_speed_header");
    BRLS_BIND(brls::Slider, mouseSpeedSlider, "mouse_speed_slider");
    BRLS_BIND(brls::Header, zoomHeader, "zoom_header");
    BRLS_BIND(brls::Slider, zoomSlider, "zoom_slider");
    BRLS_BIND(brls::Header, panXHeader, "pan_x_header");
    BRLS_BIND(brls::Slider, panXSlider, "pan_x_slider");
    BRLS_BIND(brls::Header, panYHeader, "pan_y_header");
    BRLS_BIND(brls::Slider, panYSlider, "pan_y_slider");

    BRLS_BIND(brls::BooleanCell, forwardMotion, "forward_motion");
    BRLS_BIND(brls::BooleanCell, consoleMotionFallback, "console_motion_fallback");

    BRLS_BIND(brls::BooleanCell, rememberZoomPan, "remember_zoom_pan");
    BRLS_BIND(brls::DetailCell, resetZoomPan, "reset_zoom_pan");

    BRLS_BIND(brls::BooleanCell, wireguardEnabled, "wireguard_enabled");
    BRLS_BIND(brls::DetailCell, wireguardConfigPath, "wireguard_config_path");
    BRLS_BIND(brls::DetailCell, wireguardStatus, "wireguard_status");
};
