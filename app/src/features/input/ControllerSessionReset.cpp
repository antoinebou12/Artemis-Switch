#include "ControllerSessionReset.hpp"

#include <algorithm>

namespace artemis::input {

ControllerTopologyDelta controllerTopologyDelta(int announcedCount,
                                                int connectedCount) {
    const int announced = std::max(announcedCount, 0);
    const int connected = std::max(connectedCount, 0);

    ControllerTopologyDelta delta;
    delta.changed = announced != connected;
    delta.releaseFirst = connected;
    delta.releaseEnd = std::max(announced, connected);
    delta.arriveFirst = announced;
    delta.arriveEnd = std::max(connected, announced);
    return delta;
}

} // namespace artemis::input
