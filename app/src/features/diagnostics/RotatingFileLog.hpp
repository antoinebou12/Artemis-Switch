#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace artemis::diagnostics {

// Append-only diagnostic log with a size cap and a ".1" rotation, plus
// credential redaction for the fields that routinely leak into host/client
// diagnostics. Generalizes the old VPN-only file logger so non-VPN subsystems
// can persist to the working dir with the same guarantees.
class RotatingFileLog {
public:
    enum class Severity {
        Info,
        Warning,
        Error,
    };

    static constexpr std::size_t kMaxLogBytes = 1024 * 1024;

    // Appends one redacted, timestamped line and rotates path to path + ".1"
    // before writing when the size cap would be exceeded.
    static bool append(const std::string& path,
                       std::string_view provider,
                       Severity severity,
                       std::string_view message);

    // Redacts known credential/config fields so the persisted representation
    // never carries private keys or passphrases.
    static std::string sanitize(std::string_view message);
};

} // namespace artemis::diagnostics