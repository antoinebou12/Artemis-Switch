#include "HostStreamProfileUi.hpp"

#include "HostProfileKey.hpp"
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

void open_manage_host_profile(const std::string& hostKey,
                              const std::function<void()>& onChanged) {
    auto& store = StreamConfigProfileStore::instance();
    const auto selectedId = store.selectedForHost(hostKey);
    if (selectedId.empty()) {
        showError("host/manage_profile_none"_i18n);
        return;
    }
    auto profile = store.get(selectedId);
    if (!profile) {
        showError("host/manage_profile_none"_i18n);
        return;
    }

    auto* dialog = new brls::Dialog(
        fmt::format("{} — {}", "host/manage_profile"_i18n, profile->name));
    dialog->addButton("common/edit"_i18n, [selectedId, onChanged] {
        open_edit_profile(selectedId, onChanged);
    });
    dialog->addButton("artemis/settings/duplicate_profile"_i18n,
                      [selectedId, hostKey, onChanged] {
                          auto copy =
                              StreamConfigProfileStore::instance().duplicate(
                                  selectedId, {});
                          StreamConfigProfileStore::instance().setSelectedForHost(
                              hostKey, copy.id);
                          if (onChanged)
                              onChanged();
                      });
    dialog->addButton("common/remove"_i18n, [selectedId, hostKey, onChanged] {
        auto* confirm = new brls::Dialog("host/delete_profile_message"_i18n);
        confirm->addButton("common/cancel"_i18n, [] {});
        confirm->addButton("common/remove"_i18n,
                           [selectedId, hostKey, onChanged] {
                               auto& store =
                                   StreamConfigProfileStore::instance();
                               store.remove(selectedId);
                               store.clearSelectedForHost(hostKey);
                               if (onChanged)
                                   onChanged();
                           });
        confirm->open();
    });
    dialog->addButton("common/close"_i18n, [] {});
    dialog->open();
}

} // namespace artemis::streaming
