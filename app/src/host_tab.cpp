//
//  host_tab.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#include "host_tab.hpp"
#include "GameStreamClient.hpp"
#include "app_list_view.hpp"
#include "helper.hpp"
#include "main_tabs_view.hpp"
#include "features/ui/QrCodeView.hpp"
#include "streaming/StreamProfileStore.hpp"

using namespace brls::literals;

namespace {
std::string host_subtitle(const Host& host) {
    const auto addresses = host.connection_addresses();
    if (addresses.empty()) {
        return "";
    }
    if (addresses.size() == 1) {
        return addresses.front();
    }
    return addresses.front() + " | +" +
           std::to_string(addresses.size() - 1) + " more";
}

std::string host_profile_key(const Host& host) {
    if (is_usable_mac(host.mac))
        return host.mac;
    return host.preferred_address();
}

std::string host_web_config_url(const Host& host) {
    const std::string address = host.preferred_address();
    if (address.empty())
        return {};
    return "https://" + address + ":47990/";
}
} // namespace

HostTab::HostTab(const Host& host) : host(host) {
    // Inflate the tab from the XML file
    this->inflateFromXMLRes("xml/tabs/host.xml");

    remove->setText("common/remove"_i18n);
    remove->title->setTextColor(RGB(229, 57, 53));

    reloadHost();

    hostWebConfig->setText("host/web_config"_i18n);
    hostWebConfig->setDetailText("host/web_config_hint"_i18n);
    hostWebConfig->registerClickAction([this](View*) {
        const std::string url = host_web_config_url(this->host);
        if (url.empty()) {
            showError("host/web_config_no_address"_i18n);
            return true;
        }
        Application::getPlatform()->openBrowser(url);
        artemis::ui::showUrlQrDialog("host/web_config"_i18n, url);
        return true;
    });

    streamProfile->setText("host/stream_profile"_i18n);
    {
        const auto key = host_profile_key(this->host);
        const auto profile =
            artemis::streaming::StreamProfileStore::instance().get(key);
        streamProfile->setDetailText(
            profile.customResolutionEnabled
                ? (std::to_string(profile.width) + "x" +
                   std::to_string(profile.height))
                : "host/stream_profile_global"_i18n);
        streamProfile->registerClickAction([this, key](View*) {
            const std::vector<std::string> options = {
                "host/stream_profile_global"_i18n, "720p", "1080p",
                std::to_string(Application::windowWidth) + "x" +
                    std::to_string(Application::windowHeight)};
            auto* dropdown = new Dropdown(
                "host/stream_profile"_i18n, options,
                [this, key](int selected) {
                    auto& store = artemis::streaming::StreamProfileStore::instance();
                    if (selected <= 0) {
                        store.setCustomResolution(key, false, 1920, 1080);
                        streamProfile->setDetailText(
                            "host/stream_profile_global"_i18n);
                        return;
                    }
                    int width = 1280;
                    int height = 720;
                    if (selected == 2) {
                        width = 1920;
                        height = 1080;
                    } else if (selected == 3) {
                        width = Application::windowWidth;
                        height = Application::windowHeight;
                    }
                    store.setCustomResolution(key, true, width, height);
                    streamProfile->setDetailText(std::to_string(width) + "x" +
                                                 std::to_string(height));
                },
                0);
            Application::pushActivity(new Activity(dropdown));
            return true;
        });
    }

    registerAction("host/rename"_i18n, ControllerButton::BUTTON_START,
                   [this](View* view) {
                       std::string title = this->host.hostname;
                       Application::getPlatform()->getImeManager()->openForText(
                               [this](const std::string& text) {
                                   this->host.hostname = text;
                                   Settings::instance().add_host(this->host);
                                   MainTabs::getInstanse()->refillTabs();
                               },
                               "host/rename_title"_i18n, "", 60, title, 0);

                       return true;
                   });

    connect->registerClickAction([this](View* view) {
        switch (state) {
        case AVAILABLE:
            this->present(new AppListView(this->host));
            break;
        case UNAVAILABLE:
            if (GameStreamClient::can_wake_up_host(this->host)) {
                const auto wakeRequestId = ++this->wakeRequestGeneration;
                this->canceledWakeRequestGeneration = 0;

                Dialog* loader =
                    createLoadingDialog("host/wake_up_message"_i18n,
                                        [this, wakeRequestId] {
                                            if (this->wakeRequestGeneration ==
                                                wakeRequestId) {
                                                this->canceledWakeRequestGeneration =
                                                    wakeRequestId;
                                            }
                                        });
                loader->open();

                ASYNC_RETAIN
                GameStreamClient::wake_up_host(
                    this->host,
                    [ASYNC_TOKEN, loader, wakeRequestId](
                        const GSResult<bool>& result) {
                        ASYNC_RELEASE

                        if (wakeRequestId != this->wakeRequestGeneration) {
                            return;
                        }

                        if (this->canceledWakeRequestGeneration ==
                            wakeRequestId) {
                            return;
                        }

                        loader->close([this, result, wakeRequestId] {
                            if (wakeRequestId != this->wakeRequestGeneration) {
                                return;
                            }

                            if (result.isSuccess()) {
                                reloadHost();
                            } else {
                                showError("host/wake_up_error"_i18n);
                            }
                        });
                    });
            }
            break;
        case FETCHING:
            break;
        }
        return true;
    });

    remove->registerClickAction([host](View* view) {
        auto* dialog = new Dialog("host/remove_message"_i18n);
        dialog->addButton("common/cancel"_i18n, [] {});
        dialog->addButton("common/remove"_i18n, [host] {
            Settings::instance().remove_host(host);
            MainTabs::getInstanse()->refillTabs();
        });
        dialog->open();

        return true;
    });
}

void HostTab::reloadHost() {
    state = FETCHING;
    header->setTitle("host/status"_i18n + ": " + "host/fetching"_i18n);
    header->setSubtitle(host_subtitle(host));
    connect->setText("host/wait"_i18n);

    ASYNC_RETAIN
    GameStreamClient::instance().connect(
        host, [ASYNC_TOKEN](const GSResult<SERVER_DATA>& result) {
            ASYNC_RELEASE

            if (result.isSuccess()) {
                const auto connectedAddress =
                    GameStreamClient::instance().active_address(this->host);
                header->setTitle("host/status"_i18n + ": " + "host/ready"_i18n);
                header->setSubtitle(connectedAddress.empty()
                                        ? host_subtitle(this->host)
                                        : connectedAddress);
                connect->setText("host/connect"_i18n);
                state = AVAILABLE;
            } else {
                header->setTitle("host/status"_i18n + ": " +
                                 "host/unable"_i18n);
                connect->setText("host/wake_up"_i18n);
                state = UNAVAILABLE;
            }
        });
}
