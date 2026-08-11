//
//  app_list_view.hpp
//  Moonlight
//
//  Created by Даниил Виноградов on 26.05.2021.
//

#pragma once

#include "app_cell.hpp"
#include "grid_view.hpp"
#include "loading_overlay.hpp"
#include <Settings.hpp>
#include <borealis.hpp>
#include "GameStreamClient.hpp"

#include <optional>
#include <string>

using namespace brls;

class AppListView : public Box {
  public:
    AppListView(const Host& host);

    void onLayout() override;
    void willAppear(bool resetState) override;

  private:
    enum class Pane { Applications, Host };

    Host host;
    std::string hostProfileKey;
    View* hintView = nullptr;
    DetailCell* webConfig = nullptr;
    DetailCell* streamProfile = nullptr;
    DetailCell* virtualDisplay = nullptr;
    DetailCell* appSearch = nullptr;
    Sidebar* sidebar = nullptr;
    Box* contentColumn = nullptr;
    Box* appsContainer = nullptr;
    Box* hostContainer = nullptr;
    Pane activePane = Pane::Applications;
    std::optional<AppInfo> currentApp;
    AppInfoList cachedApps;
    std::string appSearchQuery;
    int cachedCurrentGame = 0;
    bool loading = false;
    bool inputBlocked = false;
    LoadingOverlay* loader = nullptr;
    void blockInput(bool block);

    GridView* gridView = nullptr;

    void showPane(Pane pane, bool focusContent);
    void setCurrentApp(const AppInfo& app);
    void terninateApp();
    void updateAppList();
    void rebuildAppGrid();
    void promptAppSearch();
    void refreshAppSearchLabel();
    void updateFavoriteAction(AppCell* cell, Host host, const AppInfo& app);
    void refreshStreamProfileLabel();
    void refreshWebConfigVisibility();
    void refreshVirtualDisplayRow();
};
