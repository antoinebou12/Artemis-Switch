#include "artemis_settings_tab.hpp"

#include "Settings.hpp"
#include "streaming/StreamProfileStore.hpp"

#include <algorithm>
#include <fmt/format.h>

using namespace brls;

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
