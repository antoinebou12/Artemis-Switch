#pragma once

#include <string>
#include <vector>

namespace artemis::tailscale {

struct AuthKeyFileResult {
    AuthKeyFileResult() = default;
    AuthKeyFileResult(const AuthKeyFileResult&) = delete;
    AuthKeyFileResult& operator=(const AuthKeyFileResult&) = delete;
    AuthKeyFileResult(AuthKeyFileResult&& other) noexcept;
    AuthKeyFileResult& operator=(AuthKeyFileResult&& other) noexcept;
    ~AuthKeyFileResult();

    std::string key;
    std::string path;
    std::string error;

    [[nodiscard]] bool found() const noexcept { return !key.empty(); }
};

// Returns the preferred removable-storage location shown in Settings.
[[nodiscard]] std::string defaultAuthKeyPath(const std::string& workingDir);

// Searches the explicitly selected file first, then a short, deterministic
// list below working_dir()/tailscale. It never scans the full SD card.
[[nodiscard]] std::vector<std::string>
authKeyCandidatePaths(const std::string& workingDir,
                      const std::string& configuredPath);

// Reads a one-off key without logging it. Both a bare key and
// "auth_key = VALUE" files are supported. The caller owns and must wipe the
// returned key after transferring it to the control worker.
[[nodiscard]] AuthKeyFileResult
loadAuthKeyFile(const std::string& workingDir,
                const std::string& configuredPath);

} // namespace artemis::tailscale
