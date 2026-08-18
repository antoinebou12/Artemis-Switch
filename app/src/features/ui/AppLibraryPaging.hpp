#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace artemis::ui {

// The host sidebar leaves room for five 150px cards plus four 12px gaps in the
// Switch 1280px layout. Keeping pages row-aligned avoids horizontal overflow.
inline constexpr std::size_t AppLibraryColumns = 5;
inline constexpr std::size_t AppLibraryRowsPerPage = 5;
inline constexpr std::size_t AppLibraryPageSize =
    AppLibraryColumns * AppLibraryRowsPerPage;

inline std::string appLibraryNameKey(std::string_view name) {
    std::size_t first = 0;
    while (first < name.size() &&
           std::isspace(static_cast<unsigned char>(name[first])))
        ++first;

    std::size_t last = name.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(name[last - 1])))
        --last;

    std::string key;
    key.reserve(last - first);
    bool previousSpace = false;
    for (std::size_t i = first; i < last; ++i) {
        const unsigned char ch = static_cast<unsigned char>(name[i]);
        if (std::isspace(ch)) {
            if (!previousSpace)
                key.push_back(' ');
            previousSpace = true;
        } else {
            key.push_back(static_cast<char>(std::tolower(ch)));
            previousSpace = false;
        }
    }
    return key;
}

struct AppLibraryPageWindow {
    std::size_t displayed = 0;
    bool hasMore = false;
};

inline AppLibraryPageWindow appLibraryPageWindow(
    std::size_t matchingApps, std::size_t visibleLimit) {
    const std::size_t effectiveLimit =
        visibleLimit == 0 ? AppLibraryPageSize : visibleLimit;
    const std::size_t displayed = std::min(matchingApps, effectiveLimit);
    return {displayed, displayed < matchingApps};
}

inline std::size_t nextAppLibraryLimit(std::size_t matchingApps,
                                       std::size_t visibleLimit) {
    const auto current = appLibraryPageWindow(matchingApps, visibleLimit);
    return std::min(matchingApps, current.displayed + AppLibraryPageSize);
}

} // namespace artemis::ui
