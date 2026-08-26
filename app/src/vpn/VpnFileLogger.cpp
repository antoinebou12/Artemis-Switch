#include "VpnFileLogger.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace {

std::mutex& log_mutex() {
    static std::mutex mutex;
    return mutex;
}

const char* severity_name(VpnFileLogger::Severity severity) {
    switch (severity) {
    case VpnFileLogger::Severity::Info:
        return "INFO";
    case VpnFileLogger::Severity::Warning:
        return "WARN";
    case VpnFileLogger::Severity::Error:
        return "ERROR";
    }
    return "INFO";
}

std::string lowercase(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return result;
}

bool contains_secret_field(std::string_view line) {
    const auto lower = lowercase(line);
    return lower.find("privatekey") != std::string::npos ||
           lower.find("private key") != std::string::npos ||
           lower.find("presharedkey") != std::string::npos ||
           lower.find("preshared key") != std::string::npos ||
           lower.find("setupkey") != std::string::npos ||
           lower.find("setup key") != std::string::npos ||
           lower.find("wireguard secret") != std::string::npos;
}

std::string timestamp() {
    const auto now = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    const auto* local = std::localtime(&now);
    char buffer[32]{};
    if (local != nullptr && std::strftime(buffer, sizeof(buffer),
                                           "%Y-%m-%d %H:%M:%S", local) != 0) {
        return buffer;
    }
    return "0000-00-00 00:00:00";
}

} // namespace

std::string VpnFileLogger::sanitize(std::string_view message) {
    std::string result;
    std::size_t start = 0;
    while (start <= message.size()) {
        const auto end = message.find('\n', start);
        const auto lineEnd = end == std::string_view::npos ? message.size() : end;
        const auto line = message.substr(start, lineEnd - start);

        if (contains_secret_field(line)) {
            const auto equals = line.find('=');
            if (equals != std::string_view::npos) {
                result.append(line.substr(0, equals + 1));
                result.append(" <redacted>");
            } else {
                result.append("<redacted>");
            }
        } else {
            result.append(line);
        }

        if (end == std::string_view::npos)
            break;
        result.push_back('\n');
        start = end + 1;
    }
    return result;
}

bool VpnFileLogger::append(const std::string& path,
                           std::string_view provider,
                           Severity severity,
                           std::string_view message) {
    if (path.empty())
        return false;

    const auto line = "[" + timestamp() + "] [" + severity_name(severity) +
                       "] [" + std::string(provider) + "] " +
                       sanitize(message) + "\n";

    std::scoped_lock lock(log_mutex());
    std::error_code error;
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent, error);

    std::uintmax_t currentSize = 0;
    if (std::filesystem::exists(path, error)) {
        currentSize = std::filesystem::file_size(path, error);
        if (error)
            currentSize = 0;
    }

    if (currentSize > 0 && currentSize + line.size() > kMaxLogBytes) {
        const auto backup = path + ".1";
        std::filesystem::remove(backup, error);
        error.clear();
        std::filesystem::rename(path, backup, error);
        if (error)
            return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::app);
    if (!output)
        return false;
    output.write(line.data(), static_cast<std::streamsize>(line.size()));
    return output.good();
}

