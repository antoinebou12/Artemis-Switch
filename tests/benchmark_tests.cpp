#include "benchmark/BenchmarkAccumulator.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
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
    near(BenchmarkAccumulator::percentile(values, -1.0), 1.0, 1e-9, "percentile clamps low");
    near(BenchmarkAccumulator::percentile(values, 2.0), 5.0, 1e-9, "percentile clamps high");
    near(BenchmarkAccumulator::percentile({}, 0.5), 0.0, 1e-9, "empty percentile");
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

void testCounterResetDoesNotUnderflow() {
    BenchmarkAccumulator accumulator(60);
    accumulator.addSample(makeSample(1000, 60.0f, 5000, 50));
    accumulator.addSample(makeSample(2000, 60.0f, 10, 1));
    const auto summary = accumulator.summarize();

    expect(summary.receivedFrames == 0, "counter reset must not underflow received frames");
    expect(summary.networkDroppedFrames == 0, "counter reset must not underflow dropped frames");
    near(summary.networkDropPercent, 0.0, 1e-9, "counter reset does not create fake packet loss");
}

void testNonFiniteSamplesAreIgnored() {
    BenchmarkAccumulator accumulator(60);
    auto invalid = makeSample(0, std::numeric_limits<float>::quiet_NaN(), 100, 0);
    accumulator.addSample(invalid);
    accumulator.addSample(makeSample(1000, 60.0f, 160, 0));

    const auto summary = accumulator.summarize();
    near(summary.renderedFps.mean, 60.0, 1e-9, "NaN FPS sample is ignored");
    expect(std::isfinite(summary.stabilityScore), "stability score remains finite");
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

void testQueueFaultsArePenalized() {
    BenchmarkAccumulator clean(60);
    auto cleanFirst = makeSample(0, 60.0f, 1000, 0);
    auto cleanLast = makeSample(2000, 60.0f, 1120, 0);
    clean.addSample(cleanFirst);
    clean.addSample(cleanLast);

    BenchmarkAccumulator faulty(60);
    auto faultyFirst = cleanFirst;
    auto faultyLast = cleanLast;
    faultyLast.queueUnderflows = 4;
    faultyLast.queueOverflowDrops = 2;
    faultyLast.queueResyncs = 1;
    faulty.addSample(faultyFirst);
    faulty.addSample(faultyLast);

    expect(faulty.summarize().stabilityScore < clean.summarize().stabilityScore,
           "queue faults must lower stability score");
}
} // namespace

int main() {
    testPercentileInterpolation();
    testCounterDeltas();
    testCounterResetDoesNotUnderflow();
    testNonFiniteSamplesAreIgnored();
    testCleanStreamScoresHigh();
    testLossIsPenalized();
    testQueueFaultsArePenalized();

    if (failures) return EXIT_FAILURE;
    std::cout << "Artemis Switch benchmark tests passed\n";
    return EXIT_SUCCESS;
}
