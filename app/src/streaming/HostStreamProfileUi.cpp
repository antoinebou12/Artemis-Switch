#include "HostStreamProfileUi.hpp"

#include "HostProfileKey.hpp"
#include "JsonFileBrowser.hpp"
#include "ProfileEditorDialog.hpp"
#include "helper.hpp"

#include <borealis.hpp>
#include <fmt/format.h>
#include <functional>

using namespace brls::literals;

namespace artemis::streaming {
namespace {

enum class ProfileListKind { Picker, Manage };

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

void popTimesThen(int count, const std::function<void()>& then) {
    if (count <= 0) {
        if (then)
            then();
        return;
    }
    brls::Application::popActivity(
        brls::TransitionAnimation::NONE, [count, then] {
            popTimesThen(count - 1, then);
        });
}

void reopenProfileList(ProfileListKind kind, const std::string& hostKey,
                       const std::function<void()>& onChanged, int popCount) {
    // Dialog/editor dismiss animations finish first; rebuild on next tick.
    brls::sync([kind, hostKey, onChanged, popCount] {
        if (onChanged)
            onChanged();
        popTimesThen(popCount, [kind, hostKey, onChanged] {
            if (kind == ProfileListKind::Manage)
                open_manage_host_profile(hostKey, onChanged);
            else
                open_host_profile_picker(hostKey, onChanged);
        });
    });
}

void confirmDeleteProfile(const std::string& profileId,
                          const std::string& hostKey,
                          ProfileListKind kind, int popCount,
                          const std::function<void()>& onChanged) {
    auto* confirm = new brls::Dialog("host/delete_profile_message"_i18n);
    confirm->addButton("common/cancel"_i18n, [] {});
    confirm->addButton("common/remove"_i18n,
                       [profileId, hostKey, kind, popCount, onChanged] {
                           StreamConfigProfileStore::instance().remove(
                               profileId);
                           reopenProfileList(kind, hostKey, onChanged,
                                             popCount);
                       });
    confirm->open();
}

void promptRenameProfile(const std::string& profileId,
                         const std::string& currentName,
                         const std::string& hostKey, ProfileListKind kind,
                         int popCount,
                         const std::function<void()>& onChanged) {
    brls::Application::getPlatform()->getImeManager()->openForText(
        [profileId, hostKey, kind, popCount, onChanged](const std::string& text) {
            if (text.empty())
                return;
            if (!StreamConfigProfileStore::instance().rename(profileId, text))
                return;
            reopenProfileList(kind, hostKey, onChanged, popCount);
        },
        "host/rename_profile_title"_i18n, "", 40, currentName, 0);
}

void open_profile_actions_page(const StreamConfigProfile& profile,
                               const std::string& hostKey,
                               const std::function<void()>& onChanged) {
    auto* column = makePageColumn();
    const auto profileId = profile.id;
    const auto profileName = profile.name;

    addRow(column, "common/edit"_i18n)
        ->registerClickAction([profileId, hostKey, onChanged](brls::View*) {
            openProfileEditor(profileId, hostKey, onChanged);
            return true;
        });

    addRow(column, "host/rename_profile"_i18n)
        ->registerClickAction(
            [profileId, profileName, hostKey, onChanged](brls::View*) {
                // Pop actions + manage after rename so the list shows the new name.
                promptRenameProfile(profileId, profileName, hostKey,
                                    ProfileListKind::Manage, 2, onChanged);
                return true;
            });

    addRow(column, "artemis/settings/duplicate_profile"_i18n)
        ->registerClickAction([profileId, hostKey, onChanged](brls::View*) {
            auto& store = StreamConfigProfileStore::instance();
            store.duplicate(profileId, {});
            // Pop actions + manage, then reopen a fresh manage list.
            reopenProfileList(ProfileListKind::Manage, hostKey, onChanged, 2);
            return true;
        });

    addRow(column, "common/remove"_i18n)
        ->registerClickAction([profileId, hostKey, onChanged](brls::View*) {
            // Pop dialog (auto) + actions + manage after confirm.
            confirmDeleteProfile(profileId, hostKey, ProfileListKind::Manage, 2,
                                 onChanged);
            return true;
        });

    pushContentPage(
        fmt::format("{} — {}", "host/manage_profile"_i18n, profile.name),
        column);
}

void wireQuickProfileActions(brls::DetailCell* cell, const std::string& profileId,
                             const std::string& hostKey, ProfileListKind kind,
                             const std::function<void()>& onChanged) {
    cell->registerAction(
        "host/edit_profile"_i18n, brls::BUTTON_Y,
        [profileId, hostKey, onChanged](brls::View*) {
            openProfileEditor(profileId, hostKey, onChanged);
            return true;
        });
    cell->registerAction(
        "host/rename_profile"_i18n, brls::BUTTON_START,
        [profileId, hostKey, kind, onChanged](brls::View*) {
            auto profile =
                StreamConfigProfileStore::instance().get(profileId);
            if (!profile)
                return true;
            promptRenameProfile(profileId, profile->name, hostKey, kind, 1,
                                onChanged);
            return true;
        });
    cell->registerAction(
        "host/delete_profile"_i18n, brls::BUTTON_X,
        [profileId, hostKey, kind, onChanged](brls::View*) {
            // Only the list page needs popping; dialog dismisses itself.
            confirmDeleteProfile(profileId, hostKey, kind, 1, onChanged);
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
        wireQuickProfileActions(row, profile.id, hostKey,
                                ProfileListKind::Picker, onChanged);
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

    if (profiles.empty()) {
        addRow(column, "host/manage_profile_none"_i18n)->setFocusable(false);
    }

    for (const auto& profile : profiles) {
        auto* row = addRow(
            column, profile.name,
            fmt::format("{}p {}fps", profile.resolutionHeight, profile.fps));
        wireQuickProfileActions(row, profile.id, hostKey,
                                ProfileListKind::Manage, onChanged);
        // A opens Edit / Duplicate / Remove.
        row->registerClickAction([profile, hostKey, onChanged](brls::View*) {
            open_profile_actions_page(profile, hostKey, onChanged);
            return true;
        });
    }

    addRow(column, "artemis/settings/create_profile"_i18n)
        ->registerClickAction([hostKey, onChanged](brls::View*) {
            openProfileEditor({}, hostKey, [hostKey, onChanged] {
                reopenProfileList(ProfileListKind::Manage, hostKey, onChanged,
                                  1);
            });
            return true;
        });
    addRow(column, "artemis/settings/add_missing_defaults"_i18n)
        ->registerClickAction([hostKey, onChanged](brls::View*) {
            const int added =
                StreamConfigProfileStore::instance().addMissingDefaults();
            if (added > 0) {
                showAlert(
                    fmt::format("{} ({})",
                                "artemis/settings/add_missing_defaults_done"_i18n,
                                added));
                reopenProfileList(ProfileListKind::Manage, hostKey, onChanged,
                                  1);
            } else {
                showAlert("artemis/settings/add_missing_defaults_none"_i18n);
            }
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
                        reopenProfileList(ProfileListKind::Manage, hostKey,
                                          onChanged, 1);
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
