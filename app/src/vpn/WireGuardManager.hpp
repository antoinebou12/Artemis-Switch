#pragma once

#include "../utils/Singleton.hpp"
#include "WireGuardConfig.hpp"

#include <mutex>
#include <string>

struct WgNxTunnel;

class WireGuardManager : public Singleton<WireGuardManager> {
  public:
    enum class Status {
        // Not compiled in, or switched off by the user.
        Disabled,
        // Compiled in, but the linked backend is the stub: it can validate a
        // config but cannot move packets. Kept distinct from Error so the UI
        // can explain the build rather than blame the user's file.
        Unavailable,
        Stopped,
        Starting,
        Running,
        Error,
    };

    ~WireGuardManager();

    bool enable_from_settings();
    void disable();

    // Default path for the config file, used by both the startup code and the
    // settings UI so they cannot disagree about where it lives.
    static std::string default_config_path();

    [[nodiscard]] Status status() const;
    // Localized, ready to display.
    [[nodiscard]] std::string status_text() const;
    [[nodiscard]] std::string tunnel_address() const;
    [[nodiscard]] std::string last_error() const;
    [[nodiscard]] bool can_route_address(const std::string& address) const;
    bool activate_route(const std::string& address);
    bool prepare_route_for_streaming(const std::string& address);
    void deactivate_route(const std::string& address);

    // True when a real wg-nx tunnel is linked in, false for the validate-only
    // stub. Surfaced in Remote Access diagnostics so a build that cannot move
    // packets is impossible to mistake for a working one.
    [[nodiscard]] static bool backend_is_real();

  private:
    // Guards every member below. status_ used to be atomic while the two
    // strings beside it were not, which bought nothing since status_text()
    // reads both.
    mutable std::mutex mutex_;
    Status status_ = Status::Disabled;
    std::string tunnelAddress_;
    std::string lastError_;
    WgNxTunnel* tunnel_ = nullptr;

    void disable_locked();
};
