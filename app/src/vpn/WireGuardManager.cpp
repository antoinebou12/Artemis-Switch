#include "WireGuardManager.hpp"

#include "Settings.hpp"
#include "wg_nx.h"

#include <borealis.hpp>

bool WireGuardManager::enable_from_settings() {
#if !defined(__SWITCH__) || !defined(ENABLE_WIREGUARD)
    status_ = Status::Disabled;
    lastError_ = "WireGuard support is not enabled in this build";
    return false;
#else
    disable();

    const auto path = Settings::instance().wireguard_config_path();
    if (path.empty()) {
        status_ = Status::Error;
        lastError_ = "WireGuard config path is empty";
        return false;
    }

    const auto text = load_text_file(path);
    if (text.empty()) {
        status_ = Status::Error;
        lastError_ = "Could not read WireGuard config: " + path;
        return false;
    }

    status_ = Status::Starting;
    tunnel_ = wg_nx_tunnel_create(text.c_str());
    if (!tunnel_) {
        status_ = Status::Error;
        lastError_ = "Invalid WireGuard config";
        return false;
    }

    if (wg_nx_tunnel_start(tunnel_) != 0) {
        wg_nx_tunnel_destroy(tunnel_);
        tunnel_ = nullptr;
        status_ = Status::Error;
        lastError_ = "Failed to start WireGuard tunnel";
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

void WireGuardManager::disable() {
    if (tunnel_) {
        wg_nx_tunnel_destroy(tunnel_);
        tunnel_ = nullptr;
    }
    tunnelAddress_.clear();
    status_ = Status::Stopped;
}

std::string WireGuardManager::status_text() const {
    switch (status_.load()) {
    case Status::Disabled:
        return "Disabled";
    case Status::Stopped:
        return "Stopped";
    case Status::Starting:
        return "Starting";
    case Status::Running:
        return "Running";
    case Status::Error:
        return lastError_.empty() ? "Error" : lastError_;
    }
    return "Unknown";
}
