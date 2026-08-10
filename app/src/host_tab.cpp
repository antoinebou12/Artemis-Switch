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
#include "streaming/StreamConfigProfileStore.hpp"

#include <fmt/format.h>

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

std::string profile_detail_label(const std::string& hostKey) {
    auto& store = artemis::streaming::StreamConfigProfileStore::instance();
    const auto selectedId = store.selectedForHost(hostKey);
    if (selectedId.empty())
        return "host/stream_profile_global"_i18n;
    if (auto profile = store.get(selectedId))
        return profile->name;
    return "host/stream_profile_global"_i18n;
}

int height_to_picker_index(int height) {
    switch (artemis::streaming::StreamConfigProfileStore::normalizeHeight(
        height)) {
    case 360:
        return 0;
    case 480:
        return 1;
    case 1080:
        return 3;
    case 720:
    default:
        return 2;
    }
}

int picker_index_to_height(int index) {
    switch (index) {
    case 0:
        return 360;
    case 1:
        return 480;
    case 3:
        return 1080;
    case 2:
    default:
        return 720;
    }
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
        try {
            if (auto* platform = Application::getPlatform())
                platform->openBrowser(url);
        } catch (...) {
        }
        artemis::ui::showUrlQrDialog("host/web_config"_i18n, url);
        return true;
    });

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
    streamProfile->setDetailText(profile_detail_label(host_profile_key(host)));
}

void HostTab::openProfilePicker() {
    const auto key = host_profile_key(host);
    auto& store = artemis::streaming::StreamConfigProfileStore::instance();
    const auto& profiles = store.list();
    std::vector<std::string> options;
    options.reserve(profiles.size() + 1);
    options.push_back("host/stream_profile_global"_i18n);
    int selected = 0;
    const auto currentId = store.selectedForHost(key);
    for (size_t i = 0; i < profiles.size(); ++i) {
        options.push_back(profiles[i].name);
        if (!currentId.empty() && profiles[i].id == currentId)
            selected = static_cast<int>(i + 1);
    }

    auto* dropdown = new Dropdown(
        "host/stream_profile"_i18n, options,
        [this, key](int index) {
            auto& store =
                artemis::streaming::StreamConfigProfileStore::instance();
            if (index <= 0) {
                store.clearSelectedForHost(key);
            } else {
                const auto& profiles = store.list();
                const size_t profileIndex = static_cast<size_t>(index - 1);
                if (profileIndex < profiles.size())
                    store.setSelectedForHost(key, profiles[profileIndex].id);
            }
            refreshStreamProfileLabel();
        },
        selected);
    Application::pushActivity(new Activity(dropdown));
}

void HostTab::createProfileForHost() {
    Application::getPlatform()->getImeManager()->openForText(
        [this](const std::string& text) {
            if (text.empty())
                return;
            auto& store =
                artemis::streaming::StreamConfigProfileStore::instance();
            const auto profile = store.create(text, true);
            store.setSelectedForHost(host_profile_key(this->host), profile.id);
            refreshStreamProfileLabel();
        },
        "host/new_profile_title"_i18n, "", 40, "", 0);
}

void HostTab::openProfileManage() {
    const auto key = host_profile_key(host);
    auto& store = artemis::streaming::StreamConfigProfileStore::instance();
    const auto selectedId = store.selectedForHost(key);
    if (selectedId.empty()) {
        showError("host/manage_profile_none"_i18n);
        return;
    }
    auto profile = store.get(selectedId);
    if (!profile) {
        showError("host/manage_profile_none"_i18n);
        return;
    }

    auto* dialog = new Dialog(fmt::format("{} — {}", "host/manage_profile"_i18n,
                                          profile->name));
    dialog->addButton("host/rename_profile"_i18n, [this, selectedId] {
        Application::getPlatform()->getImeManager()->openForText(
            [this, selectedId](const std::string& text) {
                if (text.empty())
                    return;
                artemis::streaming::StreamConfigProfileStore::instance().rename(
                    selectedId, text);
                refreshStreamProfileLabel();
            },
            "host/rename_profile_title"_i18n, "", 40, "", 0);
    });
    dialog->addButton("host/edit_profile_resolution"_i18n,
                      [this, selectedId] {
                          auto existing = artemis::streaming::
                              StreamConfigProfileStore::instance()
                                  .get(selectedId);
                          if (!existing)
                              return;
                          const std::vector<std::string> options = {
                              "360p", "480p", "720p", "1080p"};
                          auto* dropdown = new Dropdown(
                              "host/edit_profile_resolution"_i18n, options,
                              [this, selectedId](int index) {
                                  auto profile = artemis::streaming::
                                      StreamConfigProfileStore::instance()
                                          .get(selectedId);
                                  if (!profile)
                                      return;
                                  profile->resolutionHeight =
                                      picker_index_to_height(index);
                                  artemis::streaming::
                                      StreamConfigProfileStore::instance()
                                          .update(*profile);
                                  refreshStreamProfileLabel();
                              },
                              height_to_picker_index(
                                  existing->resolutionHeight));
                          Application::pushActivity(new Activity(dropdown));
                      });
    dialog->addButton("host/snapshot_profile"_i18n, [this, selectedId] {
        auto snapshot =
            artemis::streaming::StreamConfigProfileStore::snapshotFromSettings(
                "");
        snapshot.id = selectedId;
        auto existing =
            artemis::streaming::StreamConfigProfileStore::instance().get(
                selectedId);
        if (existing)
            snapshot.name = existing->name;
        artemis::streaming::StreamConfigProfileStore::instance().update(
            snapshot);
        refreshStreamProfileLabel();
    });
    dialog->addButton("common/remove"_i18n, [this, selectedId, key] {
        auto* confirm = new Dialog("host/delete_profile_message"_i18n);
        confirm->addButton("common/cancel"_i18n, [] {});
        confirm->addButton("common/remove"_i18n, [this, selectedId, key] {
            auto& store =
                artemis::streaming::StreamConfigProfileStore::instance();
            store.remove(selectedId);
            store.clearSelectedForHost(key);
            refreshStreamProfileLabel();
        });
        confirm->open();
    });
    dialog->addButton("common/close"_i18n, [] {});
    dialog->open();
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
