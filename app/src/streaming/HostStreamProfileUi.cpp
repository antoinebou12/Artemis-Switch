#include "HostStreamProfileUi.hpp"

#include "HostProfileKey.hpp"
#include "JsonFileBrowser.hpp"
#include "ProfileEditorDialog.hpp"
#include "helper.hpp"

#include <borealis.hpp>
#include <fmt/format.h>

using namespace brls::literals;

namespace artemis::streaming {
namespace {

brls::Box* makePageColumn() {
    auto* column = new brls::Box(brls::Axis::COLUMN);
    column->setAlignItems(brls::AlignItems::STRETCH);
    column->setPadding(24, 24, 24, 24);
    return column;
}

void pushContentPage(const std::string& title, brls::Box* content) {
    auto* scroll = new brls::ScrollingFrame();
    scroll->setContentView(content);
    auto* frame = new brls::AppletFrame(scroll);
    frame->setTitle(title);
    brls::Application::pushActivity(new brls::Activity(frame));
}

brls::DetailCell* addRow(brls::Box* column, const std::string& text,
                         const std::string& detail = {}) {
    auto* cell = new brls::DetailCell();
    cell->setText(text);
    if (!detail.empty())
        cell->setDetailText(detail);
    cell->title->setSingleLine(true);
    cell->detail->setSingleLine(true);
    column->addView(cell);
    return cell;
}

void confirmDeleteProfile(const std::string& profileId,
                          const std::string& hostKey,
                          const std::function<void()>& onChanged,
                          const std::function<void()>& afterDelete) {
    auto* confirm = new brls::Dialog("host/delete_profile_message"_i18n);
    confirm->addButton("common/cancel"_i18n, [] {});
    confirm->addButton("common/remove"_i18n,
                       [profileId, hostKey, onChanged, afterDelete] {
                           auto& store = StreamConfigProfileStore::instance();
                           store.remove(profileId);
                           if (!hostKey.empty() &&
                               store.selectedForHost(hostKey) == profileId) {
                               store.clearSelectedForHost(hostKey);
                           }
                           if (onChanged)
                               onChanged();
                           if (afterDelete)
                               afterDelete();
                       });
    confirm->open();
}

void wireQuickProfileActions(brls::DetailCell* cell, const std::string& profileId,
                             const std::string& hostKey,
                             const std::function<void()>& onChanged,
                             const std::function<void()>& afterDelete = {}) {
    cell->registerAction(
        "host/edit_profile"_i18n, brls::BUTTON_Y,
        [profileId, hostKey, onChanged](brls::View*) {
            openProfileEditor(profileId, hostKey, onChanged);
            return true;
        });
    cell->registerAction(
        "host/delete_profile"_i18n, brls::BUTTON_X,
        [profileId, hostKey, onChanged, afterDelete](brls::View*) {
            confirmDeleteProfile(profileId, hostKey, onChanged, afterDelete);
            return true;
        });
}

} // namespace

std::string profile_detail_label(const std::string& hostKey) {
    auto& store = StreamConfigProfileStore::instance();
    const auto selectedId = store.selectedForHost(hostKey);
    if (selectedId.empty())
        return "host/stream_profile_global"_i18n;
    if (auto profile = store.get(selectedId))
        return profile->name;
    return "host/stream_profile_global"_i18n;
}

void open_host_profile_picker(const std::string& hostKey,
                              const std::function<void()>& onChanged) {
    auto* column = makePageColumn();
    auto& store = StreamConfigProfileStore::instance();
    const auto profiles = store.list();
    const auto currentId = store.selectedForHost(hostKey);

    addRow(column, "host/stream_profile_global"_i18n,
           currentId.empty() ? "hints/on"_i18n : "")
        ->registerClickAction([hostKey, onChanged](brls::View*) {
            StreamConfigProfileStore::instance().clearSelectedForHost(hostKey);
            if (onChanged)
                onChanged();
            brls::Application::popActivity();
            return true;
        });

    for (const auto& profile : profiles) {
        const bool selected = !currentId.empty() && profile.id == currentId;
        auto* row =
            addRow(column, profile.name, selected ? "hints/on"_i18n : "");
        // Y edit / X delete on the focused row; A still selects for this host.
        wireQuickProfileActions(
            row, profile.id, hostKey, onChanged,
            [] { brls::Application::popActivity(); });
        row->registerClickAction(
            [hostKey, id = profile.id, onChanged](brls::View*) {
                StreamConfigProfileStore::instance().setSelectedForHost(
                    hostKey, id);
                if (onChanged)
                    onChanged();
                brls::Application::popActivity();
                return true;
            });
    }

    addRow(column, "artemis/settings/manage_profiles"_i18n)
        ->registerClickAction([hostKey, onChanged](brls::View*) {
            open_manage_host_profile(hostKey, onChanged);
            return true;
        });

    pushContentPage("host/stream_profile"_i18n, column);
}

void open_create_host_profile(const std::string& hostKey,
                              const std::function<void()>& onChanged) {
    openProfileEditor({}, hostKey, onChanged);
}

void open_edit_profile(const std::string& profileId,
                       const std::function<void()>& onChanged) {
    if (profileId.empty())
        return;
    openProfileEditor(profileId, {}, onChanged);
}

void open_manage_host_profile(const std::string& hostKey,
                              const std::function<void()>& onChanged) {
    auto* column = makePageColumn();
    auto& store = StreamConfigProfileStore::instance();
    const auto profiles = store.list();

    for (const auto& profile : profiles) {
        auto* row = addRow(
            column, profile.name,
            fmt::format("{}p {}fps", profile.resolutionHeight, profile.fps));
        wireQuickProfileActions(
            row, profile.id, hostKey, onChanged,
            [hostKey, onChanged] {
                brls::Application::popActivity(
                    brls::TransitionAnimation::NONE, [hostKey, onChanged] {
                        open_manage_host_profile(hostKey, onChanged);
                    });
            });
        // A opens the editor immediately.
        row->registerClickAction(
            [id = profile.id, hostKey, onChanged](brls::View*) {
                openProfileEditor(id, hostKey, onChanged);
                return true;
            });
    }

    addRow(column, "artemis/settings/create_profile"_i18n)
        ->registerClickAction([hostKey, onChanged](brls::View*) {
            openProfileEditor({}, hostKey, onChanged);
            return true;
        });
    addRow(column, "artemis/settings/export_profiles"_i18n)
        ->registerClickAction([](brls::View*) {
            openJsonFileBrowser(
                JsonFileBrowserMode::Export, [](const std::string& path) {
                    if (StreamConfigProfileStore::instance().exportJson(path)) {
                        showAlert(fmt::format(
                            "{} {}", "artemis/settings/export_profiles_done"_i18n,
                            path));
                    } else {
                        showError(
                            "artemis/settings/export_profiles_error"_i18n);
                    }
                });
            return true;
        });
    addRow(column, "artemis/settings/import_profiles"_i18n)
        ->registerClickAction([hostKey, onChanged](brls::View*) {
            openJsonFileBrowser(
                JsonFileBrowserMode::Import,
                [hostKey, onChanged](const std::string& path) {
                    std::string error;
                    if (StreamConfigProfileStore::instance().importJson(
                            path, true, &error)) {
                        showAlert("artemis/settings/import_profiles_done"_i18n);
                        if (onChanged)
                            onChanged();
                        brls::Application::popActivity(
                            brls::TransitionAnimation::NONE, [hostKey, onChanged] {
                                open_manage_host_profile(hostKey, onChanged);
                            });
                    } else {
                        showError(
                            error.empty()
                                ? "artemis/settings/import_profiles_error"_i18n
                                : error);
                    }
                });
            return true;
        });

    pushContentPage("artemis/settings/manage_profiles"_i18n, column);
}

} // namespace artemis::streaming
