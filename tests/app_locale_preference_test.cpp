#include "AppLocalePreference.hpp"

#include <cassert>
#include <iostream>

int main() {
    using artemis::i18n::normalize_app_locale;
    using artemis::i18n::available_app_locales;

    assert(normalize_app_locale("") == "auto");
    assert(normalize_app_locale("nope") == "auto");
    assert(normalize_app_locale("auto") == "auto");
    assert(normalize_app_locale("fr") == "fr");
    assert(normalize_app_locale("en-US") == "en-US");
    assert(normalize_app_locale("zh-Hans") == "zh-Hans");

    const auto locales = available_app_locales();
    assert(locales.size() >= 10);
    assert(locales.front().code == "auto");

    bool foundFrench = false;
    for (const auto& option : locales) {
        if (option.code == "fr") {
            foundFrench = true;
            break;
        }
    }
    assert(foundFrench);

    std::cout << "app_locale_preference_test ok\n";
    return 0;
}
