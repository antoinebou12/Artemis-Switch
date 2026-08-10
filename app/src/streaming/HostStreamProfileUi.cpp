#include "HostStreamProfileUi.hpp"

#include "HostProfileKey.hpp"
#include "JsonFileBrowser.hpp"
#include "ProfileEditorDialog.hpp"
#include "helper.hpp"

#include <borealis.hpp>
#include <fmt/format.h>

using namespace brls::literals;

namespace artemis::streaming {

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
    auto& store = StreamConfigProfileStore::instance();
    const auto& profiles = store.list();
    std::vector<std::string> options;
    options.reserve(profiles.size() + 2);
    options.push_back("host/stream_profile_global"_i18n);
    int selected = 0;
    const auto currentId = store.selectedForHost(hostKey);
    for (size_t i = 0; i < profiles.size(); ++i) {
        options.push_back(profiles[i].name);
        if (!currentId.empty() && profiles[i].id == currentId)
            selected = static_cast<int>(i + 1);
    }
    options.push_back("artemis/settings/manage_profiles"_i18n);

    auto* dropdown = new brls::Dropdown(
        "host/stream_profile"_i18n, options,
        [hostKey, onChanged](int index) {
            auto& store = StreamConfigProfileStore::instance();
            const auto profiles = store.list();
            if (index <= 0) {
                store.clearSelectedForHost(hostKey);
            } else if (index == static_cast<int>(profiles.size() + 1)) {
                open_manage_host_profile(hostKey, onChanged);
                return;
            } else {
                const size_t profileIndex = static_cast<size_t>(index - 1);
                if (profileIndex < profiles.size())
                    store.setSelectedForHost(hostKey, profiles[profileIndex].id);
            }
            if (onChanged)
                onChanged();
        },
        selected);
    brls::Application::pushActivity(new brls::Activity(dropdown));
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

namespace {

void open_profile_actions(const StreamConfigProfile& profile,
                          const std::string& hostKey,
                          const std::function<void()>& onChanged) {
    auto* dialog = new brls::Dialog(
        fmt::format("{} — {}", "host/manage_profile"_i18n, profile.name));
    const auto profileId = profile.id;

    dialog->addButton("common/edit"_i18n, [profileId, onChanged] {
        brls::sync([profileId, onChanged] {
            open_edit_profile(profileId, onChanged);
        });
    });
    dialog->addButton("artemis/settings/duplicate_profile"_i18n,
                      [profileId, hostKey, onChanged] {
                          auto copy =
                              StreamConfigProfileStore::instance().duplicate(
                                  profileId, {});
                          if (!hostKey.empty()) {
                              StreamConfigProfileStore::instance()
                                  .setSelectedForHost(hostKey, copy.id);
                          }
                          if (onChanged)
                              onChanged();
                      });
    dialog->addButton("common/remove"_i18n, [profileId, hostKey, onChanged] {
        brls::sync([profileId, hostKey, onChanged] {
            auto* confirm =
                new brls::Dialog("host/delete_profile_message"_i18n);
            confirm->addButton("common/cancel"_i18n, [] {});
            confirm->addButton("common/remove"_i18n,
                               [profileId, hostKey, onChanged] {
                                   auto& store =
                                       StreamConfigProfileStore::instance();
                                   store.remove(profileId);
                                   if (!hostKey.empty() &&
                                       store.selectedForHost(hostKey) ==
                                           profileId) {
                                       store.clearSelectedForHost(hostKey);
                                   }
                                   if (onChanged)
                                       onChanged();
                               });
            confirm->open();
        });
    });
    dialog->addButton("common/close"_i18n, [] {});
    dialog->open();
}

} // namespace

void open_manage_host_profile(const std::string& hostKey,
                              const std::function<void()>& onChanged) {
    auto& store = StreamConfigProfileStore::instance();
    const auto profiles = store.list();
    std::vector<std::string> options;
    options.reserve(profiles.size() + 3);
    for (const auto& profile : profiles) {
        options.push_back(fmt::format("{} ({}p {}fps)", profile.name,
                                      profile.resolutionHeight, profile.fps));
    }
    options.push_back("artemis/settings/create_profile"_i18n);
    options.push_back("artemis/settings/export_profiles"_i18n);
    options.push_back("artemis/settings/import_profiles"_i18n);

    auto* dropdown = new brls::Dropdown(
        "artemis/settings/manage_profiles"_i18n, options,
        [hostKey, onChanged](int index) {
            auto& store = StreamConfigProfileStore::instance();
            const auto profiles = store.list();
            const int createIndex = static_cast<int>(profiles.size());
            const int exportIndex = createIndex + 1;
            const int importIndex = createIndex + 2;

            if (index >= 0 && index < createIndex) {
                open_profile_actions(profiles[static_cast<size_t>(index)],
                                     hostKey, onChanged);
                return;
            }
            if (index == createIndex) {
                brls::sync([hostKey, onChanged] {
                    openProfileEditor({}, hostKey, onChanged);
                });
                return;
            }
            if (index == exportIndex) {
                openJsonFileBrowser(
                    JsonFileBrowserMode::Export,
                    [](const std::string& path) {
                        if (StreamConfigProfileStore::instance().exportJson(
                                path)) {
                            showAlert(fmt::format(
                                "{} {}",
                                "artemis/settings/export_profiles_done"_i18n,
                                path));
                        } else {
                            showError(
                                "artemis/settings/export_profiles_error"_i18n);
                        }
                    });
                return;
            }
            if (index == importIndex) {
                openJsonFileBrowser(
                    JsonFileBrowserMode::Import,
                    [onChanged](const std::string& path) {
                        std::string error;
                        if (StreamConfigProfileStore::instance().importJson(
                                path, true, &error)) {
                            showAlert(
                                "artemis/settings/import_profiles_done"_i18n);
                            if (onChanged)
                                onChanged();
                        } else {
                            showError(
                                error.empty()
                                    ? "artemis/settings/import_profiles_error"_i18n
                                    : error);
                        }
                    });
            }
        },
        0);
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

} // namespace artemis::streaming
