#include "AutoTunePlan.hpp"

namespace artemis::benchmark {
namespace {
AutoTuneStep step(int width, int height, int bitrateKbps, int decoderThreads,
                  int warmupSeconds = 3, int benchmarkSeconds = 10) {
    AutoTuneStep result;
    result.profile = {width, height, 60, bitrateKbps, decoderThreads, "HEVC"};
    result.warmupSeconds = warmupSeconds;
    result.benchmarkSeconds = benchmarkSeconds;
    return result;
}
}

std::vector<AutoTuneStep> AutoTunePlan::quickSwitchPlan() {
    return {
        step(1280, 720, 15000, 2),
        step(1920, 1080, 15000, 2),
        step(1920, 1080, 20000, 2),
        step(1920, 1080, 25000, 2),
        step(1920, 1080, 30000, 2),
        step(1920, 1080, 20000, 4),
    };
}

std::vector<AutoTuneStep> AutoTunePlan::extendedSwitchPlan() {
    auto result = quickSwitchPlan();
    result.push_back(step(1280, 720, 20000, 2));
    result.push_back(step(1920, 1080, 35000, 2));
    result.push_back(step(1920, 1080, 40000, 2));
    result.push_back(step(1920, 1080, 25000, 3));
    result.push_back(step(1920, 1080, 25000, 4));
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
