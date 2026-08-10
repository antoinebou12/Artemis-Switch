#pragma once

#include "../utils/Singleton.hpp"
#include "WireGuardConfig.hpp"

#include <atomic>
#include <string>

struct WgNxTunnel;

class WireGuardManager : public Singleton<WireGuardManager> {
  public:
    enum class Status { Disabled, Stopped, Starting, Running, Error };

    bool enable_from_settings();
    void disable();

    [[nodiscard]] Status status() const { return status_.load(); }
    [[nodiscard]] std::string status_text() const;
    [[nodiscard]] std::string tunnel_address() const { return tunnelAddress_; }
    [[nodiscard]] std::string last_error() const { return lastError_; }

  private:
    std::atomic<Status> status_{Status::Disabled};
    std::string tunnelAddress_;
    std::string lastError_;
    WgNxTunnel* tunnel_ = nullptr;
};
