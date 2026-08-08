#pragma once

#include "AutoTunePlan.hpp"

#include <atomic>
#include <mutex>
#include <optional>
#include <thread>

namespace artemis::benchmark {

class AutoTuneRuntime {
public:
    static AutoTuneRuntime& instance();
    ~AutoTuneRuntime();

    [[nodiscard]] bool available() const;
    bool start(bool extended = false);
    void cancel();

    [[nodiscard]] bool running() const { return m_running.load(); }
    [[nodiscard]] size_t currentStep() const;
    [[nodiscard]] size_t totalSteps() const;
    [[nodiscard]] std::optional<AutoTuneResult> recommendation() const;

private:
    AutoTuneRuntime() = default;
    AutoTuneRuntime(const AutoTuneRuntime&) = delete;
    AutoTuneRuntime& operator=(const AutoTuneRuntime&) = delete;

    void worker(bool extended);

    mutable std::mutex m_mutex;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_cancelRequested{false};
    std::thread m_worker;
    AutoTuneSession m_session;
    std::optional<AutoTuneResult> m_recommendation;
};

} // namespace artemis::benchmark
