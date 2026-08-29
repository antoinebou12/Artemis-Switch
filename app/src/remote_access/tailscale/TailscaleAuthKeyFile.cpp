#include "TailscaleAuthKeyFile.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace artemis::tailscale {
namespace {

constexpr std::uintmax_t kMaximumKeyFileSize = 4096;
constexpr std::size_t kMaximumKeyLength = 1024;

std::string trim(std::string text) {
    const auto first = text.find_first_not_of(" \t\r\n\"'");
    if (first == std::string::npos)
        return {};
    const auto last = text.find_last_not_of(" \t\r\n\"'");
    return text.substr(first, last - first + 1);
}

bool validKey(std::string_view key) {
    if (key.size() < 16 || key.size() > kMaximumKeyLength)
        return false;
    return std::all_of(key.begin(), key.end(), [](unsigned char c) {
        return c >= 0x21 && c <= 0x7e;
    });
}

std::string readCandidate(const std::string& path, std::string& error) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec))
        return {};
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size == 0 || size > kMaximumKeyFileSize) {
        error = "Tailscale auth key file is empty or too large";
        return {};
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "Could not open Tailscale auth key file";
        return {};
    }

    std::string line;
    while (std::getline(file, line)) {
        line = trim(std::move(line));
        if (line.empty() || line.front() == '#' || line.front() == ';')
            continue;

        const auto separator = line.find_first_of("=:");
        std::string key = line;
        if (separator != std::string::npos) {
            std::string name = trim(line.substr(0, separator));
            std::transform(name.begin(), name.end(), name.begin(),
                           [](unsigned char c) {
                               return static_cast<char>(std::tolower(c));
                           });
            if (name != "auth_key" && name != "authkey" && name != "key")
                continue;
            key = trim(line.substr(separator + 1));
        }
        if (!validKey(key)) {
            error = "Tailscale auth key file contains an invalid key";
            return {};
        }
        return key;
    }

    error = "No Tailscale auth key was found in the selected file";
    return {};
}

} // namespace

AuthKeyFileResult::AuthKeyFileResult(AuthKeyFileResult&& other) noexcept
    : key(std::move(other.key)), path(std::move(other.path)),
      error(std::move(other.error)) {}

AuthKeyFileResult&
AuthKeyFileResult::operator=(AuthKeyFileResult&& other) noexcept {
    if (this == &other)
        return *this;
    std::fill(key.begin(), key.end(), '\0');
    key = std::move(other.key);
    path = std::move(other.path);
    error = std::move(other.error);
    return *this;
}

AuthKeyFileResult::~AuthKeyFileResult() {
    std::fill(key.begin(), key.end(), '\0');
}

std::string defaultAuthKeyPath(const std::string& workingDir) {
    return (std::filesystem::path(workingDir) / "tailscale" / "auth.key")
        .generic_string();
}

std::vector<std::string>
authKeyCandidatePaths(const std::string& workingDir,
                      const std::string& configuredPath) {
    std::vector<std::string> result;
    const auto appendUnique = [&result](std::string path) {
        if (!path.empty() &&
            std::find(result.begin(), result.end(), path) == result.end()) {
            result.push_back(std::move(path));
        }
    };
    appendUnique(configuredPath);
    const auto directory = std::filesystem::path(workingDir) / "tailscale";
    appendUnique((directory / "auth.key").generic_string());
    appendUnique((directory / "auth-key.txt").generic_string());
    appendUnique((std::filesystem::path(workingDir) / "tailscale.key")
                     .generic_string());
    return result;
}

AuthKeyFileResult loadAuthKeyFile(const std::string& workingDir,
                                 const std::string& configuredPath) {
    AuthKeyFileResult result;
    bool sawFile = false;
    for (const auto& path : authKeyCandidatePaths(workingDir, configuredPath)) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec))
            continue;
        sawFile = true;
        std::string error;
        auto key = readCandidate(path, error);
        if (!key.empty()) {
            result.key = std::move(key);
            result.path = path;
            return result;
        }
        if (result.error.empty())
            result.error = std::move(error);
    }
    if (!sawFile)
        result.error = "Tailscale auth key file was not found";
    return result;
}

} // namespace artemis::tailscale
