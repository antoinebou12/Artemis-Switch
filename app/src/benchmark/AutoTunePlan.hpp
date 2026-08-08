#pragma once

#include "AutoTune.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace artemis::benchmark {

struct AutoTuneStep {
    AutoTuneProfile profile;
    int warmupSeconds = 3;
    int benchmarkSeconds = 10;
};

class AutoTunePlan {
public:
    static std::vector<AutoTuneStep> quickSwitchPlan();
    static std::vector<AutoTuneStep> extendedSwitchPlan();
};

class AutoTuneSession {
public:
    explicit AutoTuneSession(std::vector<AutoTuneStep> steps = {});

    void reset(std::vector<AutoTuneStep> steps);
    [[nodiscard]] bool finished() const;
    [[nodiscard]] size_t currentIndex() const { return m_index; }
    [[nodiscard]] size_t totalSteps() const { return m_steps.size(); }
    [[nodiscard]] const AutoTuneStep* current() const;
    bool record(const AutoTuneResult& result);
    [[nodiscard]] const std::vector<AutoTuneResult>& results() const { return m_results; }
    [[nodiscard]] std::optional<AutoTuneResult>
    recommendation(const AutoTunePolicy& policy = {}) const;

private:
    std::vector<AutoTuneStep> m_steps;
    std::vector<AutoTuneResult> m_results;
    size_t m_index = 0;
};

} // namespace artemis::benchmark
