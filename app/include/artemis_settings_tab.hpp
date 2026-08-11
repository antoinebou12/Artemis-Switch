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
    void editBitrate();
    void refreshFrameRateSelector();

    BRLS_BIND(brls::BooleanCell, customResolution, "custom_resolution");
    BRLS_BIND(brls::DetailCell, width, "custom_width");
    BRLS_BIND(brls::DetailCell, height, "custom_height");
    BRLS_BIND(brls::DetailCell, exactBitrate, "exact_bitrate");
    BRLS_BIND(brls::SelectorCell, frameRate, "frame_rate");
    BRLS_BIND(brls::SelectorCell, resolutionPreset, "resolution_preset");
    BRLS_BIND(brls::BooleanCell, forceFullRange, "force_full_range");
    BRLS_BIND(brls::BooleanCell, preventPacketLoss, "prevent_packet_loss");
    BRLS_BIND(brls::BooleanCell, lowLatencyPacing, "low_latency_pacing");
    BRLS_BIND(brls::SelectorCell, packetSize, "packet_size");

    BRLS_BIND(brls::SelectorCell, scaleMode, "scale_mode");

    BRLS_BIND(brls::BooleanCell, forwardMotion, "forward_motion");
    BRLS_BIND(brls::BooleanCell, consoleMotionFallback, "console_motion_fallback");

    BRLS_BIND(brls::BooleanCell, rememberZoomPan, "remember_zoom_pan");
    BRLS_BIND(brls::DetailCell, resetZoomPan, "reset_zoom_pan");

    BRLS_BIND(brls::BooleanCell, wireguardEnabled, "wireguard_enabled");
    BRLS_BIND(brls::DetailCell, wireguardConfigPath, "wireguard_config_path");
    BRLS_BIND(brls::DetailCell, wireguardStatus, "wireguard_status");

    brls::Event<int>::Subscription frameRateSubscription;
    bool hasFrameRateSubscription = false;
};
