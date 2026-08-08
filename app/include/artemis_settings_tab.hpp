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

    BRLS_BIND(brls::BooleanCell, customResolution, "custom_resolution");
    BRLS_BIND(brls::DetailCell, width, "custom_width");
    BRLS_BIND(brls::DetailCell, height, "custom_height");
    BRLS_BIND(brls::DetailCell, exactBitrate, "exact_bitrate");
    BRLS_BIND(brls::DetailCell, activeProfile, "active_profile");
};
