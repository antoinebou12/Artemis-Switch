#include "JsonFileBrowser.hpp"

#include "FileBrowserPath.hpp"
#include "Settings.hpp"

#include <algorithm>
#include <borealis.hpp>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>

using namespace brls::literals;

namespace artemis::streaming {
namespace {

const std::vector<std::string>& jsonOnly() {
    static const std::vector<std::string> extensions{".json"};
    return extensions;
}

std::string normalizeBrowseRoot(std::string path) {
    if (path.empty())
        return "sdmc:/";
#ifdef __SWITCH__
    if (path.rfind("sdmc:", 0) != 0 && path.front() == '/')
        path = "sdmc:" + path;
#endif
    return path;
}

std::string parentDirectory(const std::string& path) {
    std::filesystem::path p(path);
    if (p.has_parent_path() && p.parent_path() != p) {
        auto parent = p.parent_path().string();
        if (parent.empty() || parent == "." || parent == "sdmc:")
            return "sdmc:/";
        if (parent.back() != '/' && parent != "sdmc:/")
            parent.push_back('/');
        return parent;
    }
    return "sdmc:/";
}

void browseAt(const std::string& directory, JsonFileBrowserMode mode,
              const std::vector<std::string>& extensions,
              const std::string& title,
              const std::function<void(const std::string&)>& onPicked);

void openExportFilename(const std::string& directory,
                        const std::vector<std::string>& extensions,
                        const std::function<void(const std::string&)>& onPicked) {
    const std::string defaultName =
        ensureFileExtension("profile_export", extensions);
    brls::Application::getPlatform()->getImeManager()->openForText(
        [directory, extensions, defaultName,
         onPicked](const std::string& text) {
            std::string name = text.empty() ? defaultName : text;
            name = ensureFileExtension(std::move(name), extensions);
            std::filesystem::path out =
                std::filesystem::path(directory) / name;
            if (onPicked)
                onPicked(out.string());
        },
        "artemis/settings/export_filename"_i18n, "", 64, defaultName, 0);
}

void browseAt(const std::string& directory, JsonFileBrowserMode mode,
              const std::vector<std::string>& extensions,
              const std::string& title,
              const std::function<void(const std::string&)>& onPicked) {
    std::error_code ec;
    std::vector<std::string> folders;
    std::vector<std::string> files;

    const auto dirPath = std::filesystem::path(directory);
    if (std::filesystem::exists(dirPath, ec) &&
        std::filesystem::is_directory(dirPath, ec)) {
        for (const auto& entry :
             std::filesystem::directory_iterator(dirPath, ec)) {
            if (ec)
                break;
            const auto name = entry.path().filename().string();
            if (name.empty() || name[0] == '.')
                continue;
            if (entry.is_directory(ec)) {
                folders.push_back(name);
            } else if (mode == JsonFileBrowserMode::Import &&
                       entry.is_regular_file(ec) &&
                       fileMatchesExtensions(name, extensions)) {
                files.push_back(name);
            }
        }
    }

    std::sort(folders.begin(), folders.end());
    std::sort(files.begin(), files.end());

    std::vector<std::string> options;
    options.push_back("..");
    if (mode == JsonFileBrowserMode::Export)
        options.push_back("artemis/settings/export_here"_i18n);
    for (const auto& folder : folders)
        options.push_back(folder + "/");
    for (const auto& file : files)
        options.push_back(file);

    auto* dropdown = new brls::Dropdown(
        title, options,
        [directory, mode, extensions, title, onPicked, folders,
         files](int index) {
            if (index < 0)
                return;

            int cursor = 0;
            if (index == cursor) {
                browseAt(parentDirectory(directory), mode, extensions, title,
                         onPicked);
                return;
            }
            ++cursor;

            if (mode == JsonFileBrowserMode::Export) {
                if (index == cursor) {
                    openExportFilename(directory, extensions, onPicked);
                    return;
                }
                ++cursor;
            }

            const int folderEnd =
                cursor + static_cast<int>(folders.size());
            if (index < folderEnd) {
                const auto& name =
                    folders[static_cast<size_t>(index - cursor)];
                auto next = std::filesystem::path(directory) / name;
                browseAt(next.string(), mode, extensions, title, onPicked);
                return;
            }

            const int fileIndex =
                index - folderEnd;
            if (fileIndex >= 0 &&
                fileIndex < static_cast<int>(files.size())) {
                auto path =
                    std::filesystem::path(directory) /
                    files[static_cast<size_t>(fileIndex)];
                if (onPicked)
                    onPicked(path.string());
            }
        },
        0);
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

} // namespace

void openFileBrowser(
    JsonFileBrowserMode mode, const std::vector<std::string>& extensions,
    const std::string& title,
    const std::function<void(const std::string& path)>& onPicked) {
    std::string start = Settings::instance().working_dir();
    if (start.empty())
        start = "sdmc:/";
    start = normalizeBrowseRoot(start);
    browseAt(start, mode, extensions, title, onPicked);
}

void openJsonFileBrowser(
    JsonFileBrowserMode mode,
    const std::function<void(const std::string& path)>& onPicked) {
    openFileBrowser(mode, jsonOnly(),
                    mode == JsonFileBrowserMode::Import
                        ? "artemis/settings/import_profiles"_i18n
                        : "artemis/settings/export_profiles"_i18n,
                    onPicked);
}

} // namespace artemis::streaming
