#pragma once

#include "BenchmarkAccumulator.hpp"

#include <atomic>
#include <mutex>
#include <thread>

namespace artemis::benchmark {

class BenchmarkRuntime {
public:
    static BenchmarkRuntime& instance();
    ~BenchmarkRuntime();

    bool start(int targetFps = 60);
    BenchmarkSummary stop();
    void reset();

    [[nodiscard]] bool running() const { return m_running.load(); }
    [[nodiscard]] size_t sampleCount() const;
    [[nodiscard]] BenchmarkSummary snapshot() const;

private:
    BenchmarkRuntime() = default;
    BenchmarkRuntime(const BenchmarkRuntime&) = delete;
    BenchmarkRuntime& operator=(const BenchmarkRuntime&) = delete;

    void workerLoop();
    void captureSample();

    mutable std::mutex m_mutex;
    std::atomic<bool> m_running{false};
    std::thread m_worker;
    BenchmarkAccumulator m_accumulator{60};
};

} // namespace artemis::benchmark
