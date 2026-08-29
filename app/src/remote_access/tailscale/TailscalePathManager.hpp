#pragma once

#include "TailscaleTypes.hpp"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace artemis::tailscale {

class PathManager {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto kDirectFreshness = std::chrono::seconds(15);
    static constexpr auto kProbeTimeout = std::chrono::seconds(3);

    void setDerp(std::string_view peerId, std::string region);
    void beginDirectProbe(std::string_view peerId, std::string endpoint,
                          Clock::time_point now);
    void directPong(std::string_view peerId, std::string endpoint, int rttMs,
                    Clock::time_point now);
    void noteDirectPacket(std::string_view peerId, Clock::time_point now);
    void networkChanged();
    void poll(Clock::time_point now);
    [[nodiscard]] RemotePathInfo pathInfo(std::string_view peerId) const;

private:
    struct State {
        std::string derpRegion;
        std::string directEndpoint;
        int directRttMs = -1;
        Clock::time_point probeStarted{};
        Clock::time_point lastDirectPacket{};
        bool probing = false;
        bool direct = false;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, State> paths_;
};

} // namespace artemis::tailscale
