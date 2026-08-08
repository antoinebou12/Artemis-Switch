#pragma once

#include "BenchmarkTypes.hpp"
#include <vector>

namespace artemis::benchmark {

class BenchmarkAccumulator {
public:
    explicit BenchmarkAccumulator(int targetFps = 60);

    void reset();
    void addSample(const BenchmarkSample& sample);

    [[nodiscard]] bool empty() const;
    [[nodiscard]] size_t size() const;
    [[nodiscard]] BenchmarkSummary summarize() const;

    static DistributionSummary summarizeValues(std::vector<double> values);
    static double percentile(std::vector<double> values, double percentile01);
    static double calculateStabilityScore(const BenchmarkSummary& summary,
                                          int targetFps);

private:
    int m_targetFps;
    std::vector<BenchmarkSample> m_samples;
};

} // namespace artemis::benchmark
