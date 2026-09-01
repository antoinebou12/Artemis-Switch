#include "../app/src/features/input/ControllerSessionReset.hpp"

#include <cassert>

using artemis::input::announcedCountForNewSession;
using artemis::input::controllerTopologyDelta;

namespace {

int arrivals(int announced, int connected) {
    const auto delta = controllerTopologyDelta(announced, connected);
    return delta.arriveEnd - delta.arriveFirst;
}

int releases(int announced, int connected) {
    const auto delta = controllerTopologyDelta(announced, connected);
    return delta.releaseEnd - delta.releaseFirst;
}

} // namespace

int main() {
    // Nothing connected, nothing announced: no work.
    assert(!controllerTopologyDelta(0, 0).changed);
    assert(arrivals(0, 0) == 0);
    assert(releases(0, 0) == 0);

    // Steady state within one session is silent.
    assert(!controllerTopologyDelta(2, 2).changed);
    assert(arrivals(2, 2) == 0);
    assert(releases(2, 2) == 0);

    // Pads appearing announce exactly the new slots.
    const auto grew = controllerTopologyDelta(1, 3);
    assert(grew.changed);
    assert(grew.arriveFirst == 1 && grew.arriveEnd == 3);
    assert(releases(1, 3) == 0);

    // Pads disappearing release exactly the departed slots.
    const auto shrank = controllerTopologyDelta(3, 1);
    assert(shrank.changed);
    assert(shrank.releaseFirst == 1 && shrank.releaseEnd == 3);
    assert(arrivals(3, 1) == 0);

    // The regression this guards: a second stream with the same pads still
    // connected. Carrying the previous session's announced count over leaves
    // the host with no arrival event, so it never learns the pad can rumble.
    assert(arrivals(2, 2) == 0);
    const auto reset = controllerTopologyDelta(announcedCountForNewSession(), 2);
    assert(reset.changed);
    assert(reset.arriveFirst == 0 && reset.arriveEnd == 2);
    assert(releases(announcedCountForNewSession(), 2) == 0);

    // A session that starts with no pads connected has nothing to announce and
    // must not emit a spurious release.
    assert(!controllerTopologyDelta(announcedCountForNewSession(), 0).changed);
    assert(arrivals(announcedCountForNewSession(), 0) == 0);
    assert(releases(announcedCountForNewSession(), 0) == 0);

    // Negative counts cannot produce an inverted range.
    for (int announced = -2; announced <= 4; ++announced) {
        for (int connected = -2; connected <= 4; ++connected) {
            const auto delta = controllerTopologyDelta(announced, connected);
            assert(delta.arriveEnd >= delta.arriveFirst);
            assert(delta.releaseEnd >= delta.releaseFirst);
            assert(delta.arriveFirst >= 0 && delta.releaseFirst >= 0);
        }
    }

    return 0;
}
