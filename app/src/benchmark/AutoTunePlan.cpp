#include "AutoTunePlan.hpp"

namespace artemis::benchmark {
namespace {
AutoTuneStep step(int width, int height, int bitrateKbps, int decoderThreads,
                  const char* codec = "HEVC",
                  int warmupSeconds = 3, int benchmarkSeconds = 10) {
    AutoTuneStep result;
    result.profile = {width, height, 60, bitrateKbps, decoderThreads, codec};
    result.warmupSeconds = warmupSeconds;
    result.benchmarkSeconds = benchmarkSeconds;
    return result;
}
}

std::vector<AutoTuneStep> AutoTunePlan::quickSwitchPlan() {
    // Start with the manually validated Switch OLED region. The previous plan
    // started at 15 Mbps, which skipped the low-latency 5-7 Mbps window entirely.
    return {
        step(1280, 720, 5000, 2),
        step(1280, 720, 6000, 2),
        step(1280, 720, 7000, 2),
        step(1280, 720, 8000, 2),
        step(1280, 720, 10000, 2),
        step(1920, 1080, 7000, 2),
    };
}

std::vector<AutoTuneStep> AutoTunePlan::extendedSwitchPlan() {
    auto result = quickSwitchPlan();

    // Codec crossover tests help distinguish a bitrate/network cliff from a
    // decoder cost. Keep the high end bounded because latency, not throughput,
    // is the optimization target on Switch.
    result.push_back(step(1280, 720, 5000, 2, "H264"));
    result.push_back(step(1280, 720, 7000, 2, "H264"));
    result.push_back(step(1280, 720, 10000, 2, "H264"));
    result.push_back(step(1920, 1080, 10000, 2));
    result.push_back(step(1920, 1080, 12000, 2));
    result.push_back(step(1280, 720, 7000, 4));
    return result;
}

AutoTuneSession::AutoTuneSession(std::vector<AutoTuneStep> steps)
    : m_steps(std::move(steps)) {}

void AutoTuneSession::reset(std::vector<AutoTuneStep> steps) {
    m_steps = std::move(steps);
    m_results.clear();
    m_index = 0;
}

bool AutoTuneSession::finished() const {
    return m_index >= m_steps.size();
}

const AutoTuneStep* AutoTuneSession::current() const {
    return finished() ? nullptr : &m_steps[m_index];
}

bool AutoTuneSession::record(const AutoTuneResult& result) {
    if (finished())
        return false;

    m_results.push_back(result);
    ++m_index;
    return true;
}

std::optional<AutoTuneResult>
AutoTuneSession::recommendation(const AutoTunePolicy& policy) const {
    return AutoTune::chooseBest(m_results, policy);
}

} // namespace artemis::benchmark
