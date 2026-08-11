//
//  app_list_view.cpp
//  Moonlight
//
//  Created by XITRIX on 26.05.2021.
//

#include "app_list_view.hpp"
#include "helper.hpp"
#include "main_tabs_view.hpp"
#include "features/ui/QrCodeView.hpp"
#include "streaming/HostProfileKey.hpp"
#include "streaming/HostStreamProfileUi.hpp"
#include "streaming/ProfileEditorDialog.hpp"
#include "streaming/StreamConfigProfileStore.hpp"

AppListView::AppListView(const Host& host) : Box(Axis::ROW), host(host) {
    hostProfileKey = artemis::streaming::host_profile_key(host);
    setAlignItems(AlignItems::STRETCH);

    auto* label = new brls::Label();
    label->setText(brls::Hint::getKeyIcon(ControllerButton::BUTTON_BACK) +
                   "  " + "app_list/terminate_current_app"_i18n);
    label->setFontSize(24);
    label->setMargins(4, 16, 4, 16);

    Box* holder = new Box();
    holder->addView(label);
    holder->setFocusable(true);
    holder->setCornerRadius(6);
    holder->setMargins(18, 0, 8, 0);
    holder->addGestureRecognizer(new TapGestureRecognizer(holder));

    hintView = holder;
    getAppletFrameItem()->setHintView(hintView);

    // Same width/style as Borealis TabFrame (previous Applications/Host menu).
    sidebar = new Sidebar();
    sidebar->setWidth(Application::getStyle()["brls/tab_frame/sidebar_width"]);
    sidebar->addItem("host/tab_applications"_i18n, [this](View* view) {
        if (!view->isFocused())
            return;
        showPane(Pane::Applications, false);
    });
    sidebar->addItem("host/tab_host"_i18n, [this](View* view) {
        if (!view->isFocused())
            return;
        showPane(Pane::Host, false);
    });
    addView(sidebar);

    contentColumn = new Box(Axis::COLUMN);
    contentColumn->setGrow(1.0f);
    contentColumn->setAlignItems(AlignItems::STRETCH);

    appsContainer = new Box(Axis::COLUMN);
    appsContainer->setAlignItems(AlignItems::STRETCH);
    appsContainer->setPadding(24, 24, 24, 24);
    appsContainer->setHideHighlight(true);
    appsContainer->setGrow(1.0f);
    gridView = new GridView();
    appsContainer->addView(gridView);
    contentColumn->addView(appsContainer);

    hostContainer = new Box(Axis::COLUMN);
    hostContainer->setAlignItems(AlignItems::STRETCH);
    hostContainer->setPadding(24, 24, 24, 24);
    hostContainer->setHideHighlight(true);
    hostContainer->setGrow(1.0f);
    hostContainer->setVisibility(Visibility::GONE);

    webConfig = new DetailCell();
    webConfig->setText("host/web_config"_i18n);
    webConfig->setDetailText("host/web_config_hint"_i18n);
    webConfig->title->setSingleLine(true);
    webConfig->detail->setSingleLine(true);
    webConfig->registerClickAction([this](View*) {
        const std::string address = this->host.preferred_address();
        if (address.empty()) {
            showError("host/web_config_no_address"_i18n);
            return true;
        }
        const std::string url = "https://" + address + ":47990/";
        brls::sync([url] {
            artemis::ui::showUrlQrDialog("host/web_config"_i18n, url);
        });
        return true;
    });
    hostContainer->addView(webConfig);
    refreshWebConfigVisibility();

    streamProfile = new DetailCell();
    streamProfile->setText("host/stream_profile"_i18n);
    streamProfile->title->setSingleLine(true);
    streamProfile->detail->setSingleLine(true);
    refreshStreamProfileLabel();
    streamProfile->registerClickAction([this](View*) {
        artemis::streaming::open_host_profile_picker(
            hostProfileKey, [this] { refreshStreamProfileLabel(); });
        return true;
    });
    streamProfile->registerAction(
        "host/edit_profile"_i18n, BUTTON_Y, [this](View*) {
            const auto selected =
                artemis::streaming::StreamConfigProfileStore::instance()
                    .selectedForHost(hostProfileKey);
            artemis::streaming::openProfileEditor(
                selected, hostProfileKey,
                [this] { refreshStreamProfileLabel(); });
            return true;
        });
    streamProfile->registerAction(
        "host/delete_profile"_i18n, BUTTON_X, [this](View*) {
            const auto selected =
                artemis::streaming::StreamConfigProfileStore::instance()
                    .selectedForHost(hostProfileKey);
            if (selected.empty()) {
                showError("host/manage_profile_none"_i18n);
                return true;
            }
            auto* confirm =
                new brls::Dialog("host/delete_profile_message"_i18n);
            confirm->addButton("common/cancel"_i18n, [] {});
            confirm->addButton("common/remove"_i18n, [this, selected] {
                auto& store =
                    artemis::streaming::StreamConfigProfileStore::instance();
                store.remove(selected);
                store.clearSelectedForHost(hostProfileKey);
                refreshStreamProfileLabel();
            });
            confirm->open();
            return true;
        });
    hostContainer->addView(streamProfile);
    contentColumn->addView(hostContainer);
    addView(contentColumn);

    // Match TabFrame: B from content returns focus to the side menu.
    auto backToSidebar = [this](View*) {
        if (Application::getInputType() == InputType::TOUCH)
            this->dismiss();
        else if (sidebar)
            Application::giveFocus(sidebar);
        return true;
    };
    appsContainer->registerAction("hints/back"_i18n, BUTTON_B, backToSidebar,
                                  false, false, SOUND_BACK);
    hostContainer->registerAction("hints/back"_i18n, BUTTON_B, backToSidebar,
                                  false, false, SOUND_BACK);

    loader = new LoadingOverlay(this);

    auto closeCurrentAction = [this](View*) {
        if (currentApp.has_value()) {
            this->terninateApp();
        }
        return true;
    };

    hintView->registerClickAction(closeCurrentAction);
    hintView->registerAction("", brls::ControllerButton::BUTTON_BACK,
                             closeCurrentAction, true);
    registerAction("", brls::ControllerButton::BUTTON_BACK, closeCurrentAction,
                   true);

    registerAction("app_list/reload_app_list"_i18n, BUTTON_Y, [this](View*) {
        if (activePane == Pane::Host) {
            const auto selected =
                artemis::streaming::StreamConfigProfileStore::instance()
                    .selectedForHost(hostProfileKey);
            brls::sync([this, selected] {
                if (selected.empty()) {
                    artemis::streaming::openProfileEditor(
                        {}, hostProfileKey,
                        [this] { refreshStreamProfileLabel(); });
                } else {
                    artemis::streaming::openProfileEditor(
                        selected, hostProfileKey,
                        [this] { refreshStreamProfileLabel(); });
                }
            });
            return true;
        }
        this->updateAppList();
        return true;
    });
    registerAction("host/delete_profile"_i18n, BUTTON_X, [this](View*) {
        if (activePane != Pane::Host)
            return false;
        const auto selected =
            artemis::streaming::StreamConfigProfileStore::instance()
                .selectedForHost(hostProfileKey);
        if (selected.empty()) {
            showError("host/manage_profile_none"_i18n);
            return true;
        }
        brls::sync([this, selected] {
            auto* confirm = new brls::Dialog("host/delete_profile_message"_i18n);
            confirm->addButton("common/cancel"_i18n, [] {});
            confirm->addButton("common/remove"_i18n, [this, selected] {
                auto& store =
                    artemis::streaming::StreamConfigProfileStore::instance();
                store.remove(selected);
                store.clearSelectedForHost(hostProfileKey);
                refreshStreamProfileLabel();
            });
            confirm->open();
        });
        return true;
    });
    registerAction("host/new_profile"_i18n, BUTTON_RB, [this](View*) {
        showPane(Pane::Host, true);
        if (sidebar && sidebar->getItem(1))
            Application::giveFocus(sidebar->getItem(1));
        artemis::streaming::open_create_host_profile(
            hostProfileKey, [this] { refreshStreamProfileLabel(); });
        return true;
    });
    blockInput(true);
}

void AppListView::showPane(Pane pane, bool focusContent) {
    activePane = pane;
    const bool apps = pane == Pane::Applications;
    appsContainer->setVisibility(apps ? Visibility::VISIBLE : Visibility::GONE);
    hostContainer->setVisibility(apps ? Visibility::GONE : Visibility::VISIBLE);

    setActionAvailable(BUTTON_Y, true);
    setActionAvailable(BUTTON_X, !apps);
    setActionAvailable(BUTTON_RB, true);
    updateActionHint(BUTTON_Y, apps ? "app_list/reload_app_list"_i18n
                                    : "host/edit_profile"_i18n);
    updateActionHint(BUTTON_X, "host/delete_profile"_i18n);

    if (!focusContent)
        return;

    if (apps) {
        if (gridView && !gridView->getChildren().empty())
            Application::giveFocus(gridView);
        else if (sidebar && sidebar->getItem(0))
            Application::giveFocus(sidebar->getItem(0));
    } else if (webConfig &&
               webConfig->getVisibility() == Visibility::VISIBLE) {
        Application::giveFocus(webConfig);
    } else if (streamProfile) {
        Application::giveFocus(streamProfile);
    } else if (sidebar && sidebar->getItem(1)) {
        Application::giveFocus(sidebar->getItem(1));
    }
}

void AppListView::refreshStreamProfileLabel() {
    if (!streamProfile)
        return;
    streamProfile->setDetailText(
        artemis::streaming::profile_detail_label(hostProfileKey));
}

void AppListView::refreshWebConfigVisibility() {
    if (!webConfig)
        return;
    webConfig->setVisibility(Settings::instance().show_host_web_config()
                                 ? Visibility::VISIBLE
                                 : Visibility::GONE);
}

void AppListView::blockInput(bool block) {
    if (block && !inputBlocked) {
        inputBlocked = block;
        Application::blockInputs();
    } else if (!block && inputBlocked) {
        inputBlocked = block;
        Application::unblockInputs();
    }
}

void AppListView::terninateApp() {
    if (loading)
        return;

    auto* dialog =
        new Dialog("app_list/terminate_prefix"_i18n + currentApp->name +
                   "app_list/terminate_postfix"_i18n);

    dialog->addButton("common/cancel"_i18n, [] {});

    dialog->addButton("app_list/terminate"_i18n, [this] {
        if (loading)
            return;

        loading = true;
        gridView->clearViews();
        Application::giveFocus(this);
        loader->setHidden(false);
        blockInput(true);

        ASYNC_RETAIN
        GameStreamClient::instance().quit(
            host, [ASYNC_TOKEN](const GSResult<bool>& result) {
                ASYNC_RELEASE

                loading = false;
                loader->setHidden(true);

                if (!result.isSuccess())
                    showError(result.error(), [this] {});

                updateAppList();
            });
    });

    dialog->open();
}

void AppListView::updateAppList() {
    if (loading)
        return;

    loading = true;

    gridView->clearViews();
    Application::giveFocus(this);
    loader->setHidden(false);
    currentApp = std::nullopt;
    hintView->setVisibility(Visibility::GONE);
    blockInput(true);
    refreshWebConfigVisibility();

    getAppletFrameItem()->title = host.hostname;
    updateAppletFrameItem();

    ASYNC_RETAIN
    GameStreamClient::instance().connect(
        host, [ASYNC_TOKEN](const GSResult<SERVER_DATA>& result) {
            ASYNC_RELEASE

            if (result.isSuccess()) {
                hostProfileKey = artemis::streaming::host_profile_key(
                    this->host, result.value().mac);
                refreshStreamProfileLabel();

                int currentGame = result.value().currentGame;

                ASYNC_RETAIN
                GameStreamClient::instance().applist(
                    host,
                    [ASYNC_TOKEN, currentGame](const GSResult<AppInfoList>& result) {
                        ASYNC_RELEASE

                        loading = false;
                        loader->setHidden(true);
                        blockInput(false);

                        if (result.isSuccess()) {
                            AppInfoList sortedApps = result.value();
                            const auto server = GameStreamClient::instance().server_data(host);
                            if (!server.isApollo()) {
                                std::stable_sort(
                                    sortedApps.begin(), sortedApps.end(),
                                    [this, currentGame](const AppInfo& l, const AppInfo& r) {
                                        const int lScore = (l.app_id == currentGame ? 2 : 0) +
                                            (Settings::instance().is_favorite(this->host, l.app_id) ? 1 : 0);
                                        const int rScore = (r.app_id == currentGame ? 2 : 0) +
                                            (Settings::instance().is_favorite(this->host, r.app_id) ? 1 : 0);
                                        return lScore > rScore;
                                    });
                            }

                            for (const AppInfo& app : sortedApps) {
                                if (app.app_id == currentGame)
                                    setCurrentApp(app);

                                auto* cell =
                                    new AppCell(host, app, currentGame);
                                cell->setFavorite(
                                    Settings::instance().is_favorite(
                                        host, app.app_id));
                                gridView->addView(cell);
                                this->updateFavoriteAction(cell, host, app);
                            }
                            // Prefer the normal TabFrame focus: sidebar item 0.
                            if (sidebar && sidebar->getItem(0))
                                Application::giveFocus(sidebar->getItem(0));
                            else
                                showPane(activePane, true);
                        } else {
                            showError(result.error(),
                                      [this] { this->dismiss(); });
                        }
                    });
            } else {
                blockInput(false);
                showError(result.error(), [this] { this->dismiss(); });
            }
        });
}

void AppListView::setCurrentApp(const AppInfo& app) {
    this->currentApp = app;
    hintView->setVisibility(Visibility::VISIBLE);
    getAppletFrameItem()->title =
        host.hostname + " - " + "app_list/running"_i18n + " " + app.name;
    updateAppletFrameItem();
}

void AppListView::willAppear(bool resetState) {
    Box::willAppear(resetState);
    updateAppList();
}

void AppListView::onLayout() {
    Box::onLayout();

    if (loader)
        loader->layout();
}

void AppListView::updateFavoriteAction(AppCell* cell, Host host, const AppInfo& app) {
    bool isFavorite = Settings::instance().is_favorite(host, app.app_id);
    cell->registerAction(
        isFavorite ? "app_list/unstar"_i18n : "app_list/star"_i18n, BUTTON_X,
        [this, cell, host, app](View* view) {
            bool isFavorite =
                Settings::instance().is_favorite(host, app.app_id);
            cell->setFavorite(!isFavorite);
            if (isFavorite) {
                Settings::instance().remove_favorite(host, app.app_id);
            } else {
                App thisApp{app.name, app.app_id};
                Settings::instance().add_favorite(host, thisApp);
            }
            MainTabs::getInstanse()->getFavoriteTab()->setRefreshNeeded();
            this->updateFavoriteAction(cell, host, app);
            return true;
        });
    Application::getGlobalHintsUpdateEvent()->fire();
}
