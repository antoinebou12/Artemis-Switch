#include "AutoTune.hpp"
#include "AutoTunePlan.hpp"

#include <cassert>
#include <vector>

using namespace artemis::benchmark;

int main() {
    AutoTuneResult unstable;
    unstable.profile = {1920, 1080, 60, 12000, 2, "HEVC"};
    unstable.stabilityScore = 90.0;
    unstable.networkDropPercent = 0.0;
    unstable.renderedFpsMean = 60.0;
    unstable.clientP99Ms = 5.0;
    assert(!AutoTune::isStable(unstable));
    assert(AutoTune::qualityScore(unstable) < 0.0);

    AutoTuneResult sweetSpot;
    sweetSpot.profile = {1280, 720, 60, 6000, 2, "HEVC"};
    sweetSpot.stabilityScore = 99.0;
    sweetSpot.networkDropPercent = 0.0;
    sweetSpot.renderedFpsMean = 60.0;
    sweetSpot.clientP99Ms = 4.0;

    // A higher-bitrate stream with a real latency increase must not win merely
    // because it has more pixels or bits. This protects the Switch OLED manual
    // result that 720p60 around 5-7 Mbps is the responsive operating region.
    AutoTuneResult highLatency = sweetSpot;
    highLatency.profile = {1920, 1080, 60, 12000, 2, "HEVC"};
    highLatency.clientP99Ms = 12.0;

    auto best = AutoTune::chooseBest({sweetSpot, highLatency});
    assert(best.has_value());
    assert(best->profile.width == 1280);
    assert(best->profile.height == 720);
    assert(best->profile.bitrateKbps == 6000);

    // If the higher resolution is genuinely latency-free and stable, Auto Tune
    // may still choose it. The policy is latency-first, not resolution-hostile.
    AutoTuneResult freeUpgrade = sweetSpot;
    freeUpgrade.profile = {1920, 1080, 60, 7000, 2, "HEVC"};
    freeUpgrade.clientP99Ms = 4.0;
    best = AutoTune::chooseBest({sweetSpot, freeUpgrade});
    assert(best.has_value());
    assert(best->profile.height == 1080);

    AutoTunePolicy strict;
    strict.maximumHeight = 720;
    best = AutoTune::chooseBest({freeUpgrade}, strict);
    assert(!best.has_value());

    const auto quick = AutoTunePlan::quickSwitchPlan();
    assert(quick.size() == 6);
    assert(quick.front().profile.height == 720);
    assert(quick.front().profile.bitrateKbps == 5000);
    assert(quick[1].profile.bitrateKbps == 6000);
    assert(quick[2].profile.bitrateKbps == 7000);
    assert(quick[3].profile.bitrateKbps == 8000);
    assert(quick[4].profile.bitrateKbps == 10000);
    assert(quick.back().profile.height == 1080);
    assert(quick.back().profile.bitrateKbps == 7000);

    const auto extended = AutoTunePlan::extendedSwitchPlan();
    assert(extended.size() == 12);
    assert(extended[6].profile.codec == "H264");
    assert(extended[7].profile.codec == "H264");
    assert(extended.back().profile.decoderThreads == 4);

    AutoTuneSession session(quick);
    assert(!session.finished());
    assert(session.currentIndex() == 0);
    assert(session.current()->profile.height == 720);

    AutoTuneResult first = sweetSpot;
    first.profile = quick.front().profile;
    assert(session.record(first));
    assert(session.currentIndex() == 1);

    for (size_t i = 1; i < quick.size(); ++i) {
        AutoTuneResult result = sweetSpot;
        result.profile = quick[i].profile;
        result.stabilityScore = 98.0;
        result.renderedFpsMean = 60.0;
        result.networkDropPercent = 0.0;
        result.clientP99Ms = result.profile.bitrateKbps > 7000 ? 10.0 : 5.0;
        assert(session.record(result));
    }
    assert(session.finished());
    assert(session.results().size() == quick.size());
    assert(session.recommendation().has_value());
    assert(session.recommendation()->profile.bitrateKbps <= 7000);

    return 0;
}
