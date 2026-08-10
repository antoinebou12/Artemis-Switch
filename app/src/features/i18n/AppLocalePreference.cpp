#include "AppLocalePreference.hpp"

#include <cstdlib>
#include <filesystem>

namespace artemis::i18n {
namespace {

bool is_supported_locale(const std::string& code) {
    if (code == "auto") {
        return true;
    }
    for (const auto& option : available_app_locales()) {
        if (option.code == code) {
            return true;
        }
    }
    return false;
}

} // namespace

std::vector<AppLocaleOption> available_app_locales() {
    return {
        {"auto", "System"},
        {"en-US", "English"},
        {"fr", "Français"},
        {"de", "Deutsch"},
        {"es", "Español"},
        {"it", "Italiano"},
        {"ja", "日本語"},
        {"ko", "한국어"},
        {"pt-BR", "Português (Brasil)"},
        {"ru", "Русский"},
        {"zh-Hans", "简体中文"},
        {"zh-Hant", "繁體中文"},
    };
}

std::string normalize_app_locale(const std::string& value) {
    if (value.empty() || !is_supported_locale(value)) {
        return "auto";
    }
    return value;
}

std::vector<std::string> settings_path_candidates() {
    std::vector<std::string> paths;

#ifdef __SWITCH__
    paths.emplace_back("sdmc:/switch/Moonlight-Switch/settings.json");
#else
    if (const char* home = std::getenv("HOME")) {
        paths.push_back((std::filesystem::path(home) / "Moonlight-Switch" /
                         "settings.json")
                            .string());
    }
#if defined(_WIN32)
    if (const char* profile = std::getenv("USERPROFILE")) {
        paths.push_back((std::filesystem::path(profile) / "Moonlight-Switch" /
                         "settings.json")
                            .string());
    }
#endif
#endif

    return paths;
}

} // namespace artemis::i18n
