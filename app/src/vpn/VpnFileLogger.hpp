#pragma once

#include <cstddef>
#include <string>
#include <string_view>

class VpnFileLogger {
public:
    enum class Severity {
        Info,
        Warning,
        Error,
    };

    static constexpr std::size_t kMaxLogBytes = 1024 * 1024;

    // Appends one sanitized diagnostic line and rotates path to path + ".1"
    // before writing when the size cap would be exceeded.
    static bool append(const std::string& path,
                       std::string_view provider,
                       Severity severity,
                       std::string_view message);

    // Redacts known credential/config fields for callers that need to inspect
    // or test the exact persisted representation.
    static std::string sanitize(std::string_view message);
};

