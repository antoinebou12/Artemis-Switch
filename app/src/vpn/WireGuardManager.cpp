#include "WireGuardManager.hpp"

#include "Settings.hpp"
#include "WireGuardConfig.hpp"
#include "wg_nx.h"

#include <borealis.hpp>

using namespace brls::literals;

namespace {
constexpr const char* kConfigFileName = "wg0.conf";
}

std::string WireGuardManager::default_config_path() {
    // Settings::working_dir() is where every other Artemis state file lives, so
    // the conf sits beside them. The settings UI shows this same value as its
    // placeholder -- previously the UI advertised a hardcoded sdmc: path while
    // startup actually used a different one.
    return Settings::instance().working_dir() + "/" + kConfigFileName;
}

WireGuardManager::~WireGuardManager() {
    std::scoped_lock lock(mutex_);
    disable_locked();
}

bool WireGuardManager::enable_from_settings() {
#if !defined(__SWITCH__) || !defined(ENABLE_WIREGUARD)
    std::scoped_lock lock(mutex_);
    disable_locked();
    status_ = Status::Disabled;
    lastError_.clear();
    return false;
#else
    std::scoped_lock lock(mutex_);
    disable_locked();

    auto path = Settings::instance().wireguard_config_path();
    if (path.empty()) {
        path = default_config_path();
    }

    auto text = load_text_file(path);
    if (text.empty()) {
        status_ = Status::Error;
        lastError_ = "artemis/settings/wireguard_error_unreadable";
        return false;
    }

    // Validate before handing anything to the backend so the user gets a
    // specific reason rather than a generic "invalid config".
    const auto config = parse_wireguard_conf(text);
    const auto problem = config.validate();
    if (problem != WireGuardConfig::Problem::None) {
        wireguard_scrub(text);
        status_ = Status::Error;
        lastError_ = wireguard_problem_i18n_key(problem);
        return false;
    }

    // The config is good. If the linked backend cannot actually tunnel, say so
    // plainly instead of reporting a tunnel that does not exist.
    if (!wg_nx_is_real_backend()) {
        wireguard_scrub(text);
        status_ = Status::Unavailable;
        lastError_.clear();
        brls::Logger::warning(
            "WireGuard: config at {} is valid, but this build links the stub "
            "backend and cannot tunnel traffic",
            path);
        return false;
    }

    status_ = Status::Starting;
    tunnel_ = wg_nx_tunnel_create(text.c_str());
    wireguard_scrub(text);
    if (!tunnel_) {
        status_ = Status::Error;
        lastError_ = "artemis/settings/wireguard_error_rejected";
        return false;
    }

    if (wg_nx_tunnel_start(tunnel_) != 0) {
        wg_nx_tunnel_destroy(tunnel_);
        tunnel_ = nullptr;
        status_ = Status::Error;
        lastError_ = "artemis/settings/wireguard_error_start_failed";
        return false;
    }

    if (const char* address = wg_nx_tunnel_address(tunnel_)) {
        tunnelAddress_ = address;
        // Prefer the interface address without CIDR for host endpoints.
        const auto slash = tunnelAddress_.find('/');
        if (slash != std::string::npos) {
            tunnelAddress_ = tunnelAddress_.substr(0, slash);
        }
    }

    status_ = Status::Running;
    lastError_.clear();
    brls::Logger::info("WireGuard: tunnel running, address={}", tunnelAddress_);
    return true;
#endif
}

void WireGuardManager::disable_locked() {
    if (tunnel_) {
        wg_nx_tunnel_destroy(tunnel_);
        tunnel_ = nullptr;
    }
    tunnelAddress_.clear();
    lastError_.clear();
}

void WireGuardManager::disable() {
    std::scoped_lock lock(mutex_);
    disable_locked();
#if !defined(__SWITCH__) || !defined(ENABLE_WIREGUARD)
    // Builds without WireGuard stay Disabled; reporting "Stopped" implied the
    // feature was merely switched off and could be switched back on.
    status_ = Status::Disabled;
#else
    status_ = Status::Stopped;
#endif
}

WireGuardManager::Status WireGuardManager::status() const {
    std::scoped_lock lock(mutex_);
    return status_;
}

std::string WireGuardManager::tunnel_address() const {
    std::scoped_lock lock(mutex_);
    return tunnelAddress_;
}

std::string WireGuardManager::last_error() const {
    std::scoped_lock lock(mutex_);
    return lastError_;
}

std::string WireGuardManager::status_text() const {
    std::scoped_lock lock(mutex_);
    switch (status_) {
    case Status::Disabled:
        return "artemis/settings/wireguard_status_disabled"_i18n;
    case Status::Unavailable:
        return "artemis/settings/wireguard_status_unavailable"_i18n;
    case Status::Stopped:
        return "artemis/settings/wireguard_status_stopped"_i18n;
    case Status::Starting:
        return "artemis/settings/wireguard_status_starting"_i18n;
    case Status::Running:
        return tunnelAddress_.empty()
                   ? "artemis/settings/wireguard_status_running"_i18n
                   : "artemis/settings/wireguard_status_running"_i18n + " · " +
                         tunnelAddress_;
    case Status::Error:
        // lastError_ holds an i18n key so the reason is translatable.
        return lastError_.empty()
                   ? "artemis/settings/wireguard_status_error"_i18n
                   : brls::getStr(lastError_);
    }
    return "artemis/settings/wireguard_status_error"_i18n;
}
