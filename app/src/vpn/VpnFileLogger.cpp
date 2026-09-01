#include "VpnFileLogger.hpp"

#include "features/diagnostics/RotatingFileLog.hpp"

// The VPN logger is now a thin, behaviour-preserving delegate over the
// generalized RotatingFileLog so non-VPN subsystems can reuse the same
// redaction and rotation. The public API is unchanged; the VPN regression test
// guards it.

std::string VpnFileLogger::sanitize(std::string_view message) {
    return artemis::diagnostics::RotatingFileLog::sanitize(message);
}

bool VpnFileLogger::append(const std::string& path,
                           std::string_view provider,
                           Severity severity,
                           std::string_view message) {
    return artemis::diagnostics::RotatingFileLog::append(
        path, provider, static_cast<artemis::diagnostics::RotatingFileLog::Severity>(severity),
        message);
}