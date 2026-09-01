#pragma once

namespace artemis::input {

// Which controller slots need an event this frame because the set of connected
// pads changed since the last one.
//
// Slots in [releaseFirst, releaseEnd) disconnected and get a neutral state.
// Slots in [arriveFirst, arriveEnd) are new and get LiSendControllerArrivalEvent,
// which is how the host learns a pad can rumble and report motion and battery.
struct ControllerTopologyDelta {
    int releaseFirst = 0;
    int releaseEnd = 0;
    int arriveFirst = 0;
    int arriveEnd = 0;
    bool changed = false;
};

ControllerTopologyDelta controllerTopologyDelta(int announcedCount,
                                                int connectedCount);

// A new Moonlight session starts with the host knowing nothing about our pads,
// so the announced count has to go back to zero even when the same controllers
// stay physically connected. Carrying the previous session's count over is what
// used to leave the second stream of a run without rumble or motion.
constexpr int announcedCountForNewSession() { return 0; }

} // namespace artemis::input
