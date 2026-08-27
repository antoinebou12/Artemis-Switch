#pragma once

#include <borealis.hpp>

#include <atomic>
#include <memory>

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

    BRLS_BIND(brls::SelectorCell, remoteAccessProvider, "remote_access_provider");
    BRLS_BIND(brls::DetailCell, wireguardConfigPath, "wireguard_config_path");
    BRLS_BIND(brls::DetailCell, netbirdServer, "netbird_server");
    BRLS_BIND(brls::DetailCell, netbirdSetupKey, "netbird_setup_key");
    BRLS_BIND(brls::BooleanCell, remoteAccessPreferLan, "remote_access_prefer_lan");
    BRLS_BIND(brls::BooleanCell, remoteAccessAutoConnect, "remote_access_auto_connect");
    BRLS_BIND(brls::DetailCell, remoteAccessAction, "remote_access_action");
    BRLS_BIND(brls::Header, remoteAccessStatusHeader, "remote_access_status_header");
    BRLS_BIND(brls::DetailCell, wireguardStatus, "wireguard_status");
    BRLS_BIND(brls::DetailCell, remoteAccessAddress, "remote_access_address");
    BRLS_BIND(brls::DetailCell, remoteAccessPeers, "remote_access_peers");
    BRLS_BIND(brls::DetailCell, remoteAccessError, "remote_access_error");
    BRLS_BIND(brls::DetailCell, wireguardBackendStatus, "wireguard_backend_status");
    BRLS_BIND(brls::DetailCell, netbirdBackendStatus, "netbird_backend_status");

  private:
    // Rows that only apply to one provider are hidden for the other, so the
    // list never offers a NetBird setup key next to a WireGuard config path.
    void refreshRemoteAccessRows();
    // Status only (address, peers, error). Safe to call on a timer.
    void refreshRemoteAccessStatus();
    void editNetBirdSetupKey();

    // Connecting is asynchronous, so the status rows are polled while this tab
    // is on screen rather than being written once at construction.
    brls::RepeatingTask* remoteAccessStatusTask_ = nullptr;

    // Cleared in the destructor. Every async continuation checks this before
    // touching the bound rows, so closing the tab mid-connect cannot write into
    // freed memory.
    std::shared_ptr<std::atomic<bool>> alive_ =
        std::make_shared<std::atomic<bool>>(true);
};
