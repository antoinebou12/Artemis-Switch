#pragma once

#include <string>
#include <vector>

namespace artemis::i18n {

struct AppLocaleOption {
    std::string code;
    std::string label;
};

// Returns supported UI locales shipped under resources/i18n.
std::vector<AppLocaleOption> available_app_locales();

// Normalizes persisted values; unknown codes fall back to "auto".
std::string normalize_app_locale(const std::string& value);

// Candidate settings.json locations used before Application::init().
std::vector<std::string> settings_path_candidates();

} // namespace artemis::i18n
