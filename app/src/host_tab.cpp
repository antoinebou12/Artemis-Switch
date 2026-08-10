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
#include "streaming/HostProfileKey.hpp"
#include "streaming/HostStreamProfileUi.hpp"

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
} // namespace

HostTab::HostTab(const Host& host) : host(host) {
    this->inflateFromXMLRes("xml/tabs/host.xml");

    remove->setText("common/remove"_i18n);
    remove->title->setTextColor(RGB(229, 57, 53));

    reloadHost();

    streamProfile->setText("host/stream_profile"_i18n);
    refreshStreamProfileLabel();
    streamProfile->registerClickAction([this](View*) {
        openProfilePicker();
        return true;
    });

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

    registerAction("host/new_profile"_i18n, ControllerButton::BUTTON_RB,
                   [this](View*) {
                       if (state != AVAILABLE)
                           return true;
                       createProfileForHost();
                       return true;
                   });

    registerAction("host/manage_profile"_i18n, ControllerButton::BUTTON_X,
                   [this](View*) {
                       if (state != AVAILABLE)
                           return true;
                       openProfileManage();
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

void HostTab::refreshStreamProfileLabel() {
    streamProfile->setDetailText(artemis::streaming::profile_detail_label(
        artemis::streaming::host_profile_key(host)));
}

void HostTab::openProfilePicker() {
    artemis::streaming::open_host_profile_picker(
        artemis::streaming::host_profile_key(host),
        [this] { refreshStreamProfileLabel(); });
}

void HostTab::createProfileForHost() {
    artemis::streaming::open_create_host_profile(
        artemis::streaming::host_profile_key(host),
        [this] { refreshStreamProfileLabel(); });
}

void HostTab::openProfileManage() {
    artemis::streaming::open_manage_host_profile(
        artemis::streaming::host_profile_key(host),
        [this] { refreshStreamProfileLabel(); });
}

void HostTab::reloadHost() {
    state = FETCHING;
    header->setTitle("host/status"_i18n + ": " + "host/fetching"_i18n);
    header->setSubtitle(host_subtitle(host));
    connect->setText("host/wait"_i18n);
    setActionAvailable(ControllerButton::BUTTON_RB, false);
    setActionAvailable(ControllerButton::BUTTON_X, false);

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
                setActionAvailable(ControllerButton::BUTTON_RB, true);
                setActionAvailable(ControllerButton::BUTTON_X, true);
            } else {
                header->setTitle("host/status"_i18n + ": " +
                                 "host/unable"_i18n);
                connect->setText("host/wake_up"_i18n);
                state = UNAVAILABLE;
                setActionAvailable(ControllerButton::BUTTON_RB, false);
                setActionAvailable(ControllerButton::BUTTON_X, false);
            }
        });
}
