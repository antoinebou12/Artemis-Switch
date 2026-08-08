#include "BenchmarkAccumulator.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace artemis::benchmark {
namespace {

template <typename Getter>
std::vector<double> collect(const std::vector<BenchmarkSample>& samples,
                            Getter getter) {
    std::vector<double> values;
    values.reserve(samples.size());
    for (const auto& sample : samples) {
        const double value = static_cast<double>(getter(sample));
        if (std::isfinite(value))
            values.push_back(value);
    }
    return values;
}

uint64_t deltaCounter(uint64_t first, uint64_t last) {
    return last >= first ? last - first : 0;
}

} // namespace

BenchmarkAccumulator::BenchmarkAccumulator(int targetFps)
    : m_targetFps(std::max(targetFps, 1)) {}

void BenchmarkAccumulator::reset() { m_samples.clear(); }

void BenchmarkAccumulator::addSample(const BenchmarkSample& sample) {
    m_samples.push_back(sample);
}

bool BenchmarkAccumulator::empty() const { return m_samples.empty(); }
size_t BenchmarkAccumulator::size() const { return m_samples.size(); }

double BenchmarkAccumulator::percentile(std::vector<double> values,
                                        double percentile01) {
    if (values.empty()) return 0.0;
    percentile01 = std::clamp(percentile01, 0.0, 1.0);
    std::sort(values.begin(), values.end());
    if (values.size() == 1) return values.front();

    const double index = percentile01 * static_cast<double>(values.size() - 1);
    const size_t lower = static_cast<size_t>(std::floor(index));
    const size_t upper = static_cast<size_t>(std::ceil(index));
    if (lower == upper) return values[lower];

    const double weight = index - static_cast<double>(lower);
    return values[lower] * (1.0 - weight) + values[upper] * weight;
}

DistributionSummary BenchmarkAccumulator::summarizeValues(std::vector<double> values) {
    DistributionSummary result;
    if (values.empty()) return result;

    const auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());
    result.min = *minIt;
    result.max = *maxIt;
    result.mean = std::accumulate(values.begin(), values.end(), 0.0) /
                  static_cast<double>(values.size());
    result.median = percentile(values, 0.50);
    result.p95 = percentile(values, 0.95);
    result.p99 = percentile(values, 0.99);
    return result;
}

BenchmarkSummary BenchmarkAccumulator::summarize() const {
    BenchmarkSummary result;
    result.sampleCount = m_samples.size();
    if (m_samples.empty()) return result;

    result.hostFps = summarizeValues(collect(m_samples, [](const auto& s){ return s.hostFps; }));
    result.receivedFps = summarizeValues(collect(m_samples, [](const auto& s){ return s.receivedFps; }));
    result.decodedFps = summarizeValues(collect(m_samples, [](const auto& s){ return s.decodedFps; }));
    result.renderedFps = summarizeValues(collect(m_samples, [](const auto& s){ return s.renderedFps; }));
    result.receiveMs = summarizeValues(collect(m_samples, [](const auto& s){ return s.receiveMs; }));
    result.decodeMs = summarizeValues(collect(m_samples, [](const auto& s){ return s.decodeMs; }));
    result.decoderDelayMs = summarizeValues(collect(m_samples, [](const auto& s){ return s.decoderDelayMs; }));
    result.renderMs = summarizeValues(collect(m_samples, [](const auto& s){ return s.renderMs; }));
    result.gpuRenderMs = summarizeValues(collect(m_samples, [](const auto& s){ return s.gpuRenderMs; }));
    result.clientProcessingMs = summarizeValues(collect(m_samples, [](const auto& s){
        return static_cast<double>(s.receiveMs) + static_cast<double>(s.decodeMs) +
               static_cast<double>(s.decoderDelayMs) + static_cast<double>(s.renderMs);
    }));

    const auto& first = m_samples.front();
    const auto& last = m_samples.back();
    if (last.timestampMs >= first.timestampMs)
        result.durationSeconds = static_cast<double>(last.timestampMs - first.timestampMs) / 1000.0;

    result.receivedFrames = deltaCounter(first.totalReceivedFrames, last.totalReceivedFrames);
    result.networkDroppedFrames = deltaCounter(first.networkDroppedFrames, last.networkDroppedFrames);
    const uint64_t networkTotal = result.receivedFrames + result.networkDroppedFrames;
    if (networkTotal > 0)
        result.networkDropPercent = static_cast<double>(result.networkDroppedFrames) * 100.0 /
                                    static_cast<double>(networkTotal);

    result.queueUnderflows = deltaCounter(first.queueUnderflows, last.queueUnderflows);
    result.queueSkippedFrames = deltaCounter(first.queueSkippedFrames, last.queueSkippedFrames);
    result.queueOverflowDrops = deltaCounter(first.queueOverflowDrops, last.queueOverflowDrops);
    result.queuePacingSkips = deltaCounter(first.queuePacingSkips, last.queuePacingSkips);
    result.queueResyncs = deltaCounter(first.queueResyncs, last.queueResyncs);
    result.stabilityScore = calculateStabilityScore(result, m_targetFps);
    return result;
}

double BenchmarkAccumulator::calculateStabilityScore(const BenchmarkSummary& summary,
                                                       int targetFps) {
    if (summary.sampleCount == 0 || targetFps <= 0) return 0.0;

    double score = 100.0;
    const double fpsRatio = summary.renderedFps.mean / static_cast<double>(targetFps);
    if (fpsRatio < 0.995)
        score -= std::min(35.0, (0.995 - fpsRatio) * 140.0);

    score -= std::min(45.0, summary.networkDropPercent * 25.0);

    const double frameBudgetMs = 1000.0 / static_cast<double>(targetFps);
    const double p99Ratio = summary.clientProcessingMs.p99 / frameBudgetMs;
    if (p99Ratio > 0.55)
        score -= std::min(20.0, (p99Ratio - 0.55) * 30.0);

    const uint64_t queueFaults = summary.queueUnderflows + summary.queueOverflowDrops +
                                 summary.queueResyncs + summary.queueSkippedFrames;
    score -= std::min(15.0, static_cast<double>(queueFaults) * 0.75);

    return std::clamp(score, 0.0, 100.0);
}

} // namespace artemis::benchmark
