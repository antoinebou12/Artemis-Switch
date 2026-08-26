#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

namespace artemis::streaming {

inline std::string normalizedFileExtension(std::string extension) {
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    if (!extension.empty() && extension.front() != '.')
        extension.insert(extension.begin(), '.');
    return extension;
}

inline bool fileMatchesExtensions(
    std::string name, const std::vector<std::string>& allowedExtensions) {
    if (allowedExtensions.empty())
        return true;

    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    for (auto extension : allowedExtensions) {
        extension = normalizedFileExtension(std::move(extension));
        if (name.size() >= extension.size() &&
            name.compare(name.size() - extension.size(), extension.size(),
                         extension) == 0) {
            return true;
        }
    }
    return false;
}

inline std::string ensureFileExtension(
    std::string name, const std::vector<std::string>& allowedExtensions) {
    if (allowedExtensions.empty() ||
        fileMatchesExtensions(name, allowedExtensions)) {
        return name;
    }
    const std::string extension =
        normalizedFileExtension(allowedExtensions.front());
    return extension.empty() ? name : name + extension;
}

} // namespace artemis::streaming
