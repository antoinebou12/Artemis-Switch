#include "AutoTune.hpp"

#include <cassert>
#include <vector>

using namespace artemis::benchmark;

int main() {
    AutoTuneResult unstable;
    unstable.profile = {1920, 1080, 60, 30000, 2, "HEVC"};
    unstable.stabilityScore = 90.0;
    unstable.networkDropPercent = 0.0;
    unstable.renderedFpsMean = 60.0;
    unstable.clientP99Ms = 5.0;
    assert(!AutoTune::isStable(unstable));
    assert(AutoTune::qualityScore(unstable) < 0.0);

    AutoTuneResult low;
    low.profile = {1280, 720, 60, 15000, 2, "HEVC"};
    low.stabilityScore = 99.0;
    low.networkDropPercent = 0.0;
    low.renderedFpsMean = 60.0;
    low.clientP99Ms = 4.0;

    AutoTuneResult high = low;
    high.profile = {1920, 1080, 60, 20000, 2, "HEVC"};

    auto best = AutoTune::chooseBest({low, high});
    assert(best.has_value());
    assert(best->profile.width == 1920);
    assert(best->profile.height == 1080);

    AutoTunePolicy strict;
    strict.maximumHeight = 720;
    best = AutoTune::chooseBest({high}, strict);
    assert(!best.has_value());

    return 0;
}
