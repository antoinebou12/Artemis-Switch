#include "benchmark/BenchmarkAccumulator.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace artemis::benchmark;

namespace {
int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void near(double actual, double expected, double epsilon, const std::string& message) {
    expect(std::fabs(actual - expected) <= epsilon, message);
}

BenchmarkSample makeSample(uint64_t timestampMs, float renderedFps,
                           uint64_t received, uint64_t dropped) {
    BenchmarkSample s;
    s.timestampMs = timestampMs;
    s.hostFps = 60.0f;
    s.receivedFps = 60.0f;
    s.decodedFps = 60.0f;
    s.renderedFps = renderedFps;
    s.receiveMs = 1.0f;
    s.decodeMs = 3.0f;
    s.decoderDelayMs = 0.5f;
    s.renderMs = 0.8f;
    s.totalReceivedFrames = received;
    s.networkDroppedFrames = dropped;
    return s;
}

void testPercentileInterpolation() {
    const std::vector<double> values{1, 2, 3, 4, 5};
    near(BenchmarkAccumulator::percentile(values, 0.50), 3.0, 1e-9, "median");
    near(BenchmarkAccumulator::percentile(values, 0.95), 4.8, 1e-9, "p95");
}

void testCounterDeltas() {
    BenchmarkAccumulator accumulator(60);
    accumulator.addSample(makeSample(1000, 60.0f, 1000, 5));
    accumulator.addSample(makeSample(3000, 60.0f, 1120, 6));
    const auto summary = accumulator.summarize();

    expect(summary.receivedFrames == 120, "received frame counter uses delta");
    expect(summary.networkDroppedFrames == 1, "network drop counter uses delta");
    near(summary.durationSeconds, 2.0, 1e-9, "duration");
    near(summary.networkDropPercent, 100.0 / 121.0, 1e-6, "drop percentage");
}

void testCleanStreamScoresHigh() {
    BenchmarkAccumulator accumulator(60);
    accumulator.addSample(makeSample(0, 60.0f, 100, 0));
    accumulator.addSample(makeSample(1000, 60.0f, 160, 0));
    accumulator.addSample(makeSample(2000, 59.99f, 220, 0));
    expect(accumulator.summarize().stabilityScore >= 99.0,
           "clean 60 FPS stream should score near 100");
}

void testLossIsPenalized() {
    BenchmarkAccumulator accumulator(60);
    accumulator.addSample(makeSample(0, 60.0f, 1000, 0));
    accumulator.addSample(makeSample(2000, 60.0f, 1120, 3));
    expect(accumulator.summarize().stabilityScore < 60.0,
           "multi-percent network loss should make profile undesirable");
}
} // namespace

int main() {
    testPercentileInterpolation();
    testCounterDeltas();
    testCleanStreamScoresHigh();
    testLossIsPenalized();

    if (failures) return EXIT_FAILURE;
    std::cout << "Artemis Switch benchmark tests passed\n";
    return EXIT_SUCCESS;
}
