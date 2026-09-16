#pragma once

#include <chrono>
#include <concepts>
#include <cstddef>
#include <deque>

namespace artemis::streaming {

// Preserve the oldest deadline until a frame can actually be released. Moving
// to the newest frame before checking the gate can continually move that
// deadline forward as new frames arrive, starving presentation.
template <class Frame, class DropFrame>
    requires std::invocable<DropFrame&, Frame&>
bool prepareLatestFrame(std::deque<Frame>& queue,
                        std::chrono::steady_clock::time_point now,
                        std::chrono::nanoseconds lead,
                        std::size_t targetBufferedFrames,
                        DropFrame&& dropFrame) {
    if (queue.empty()) {
        return false;
    }

    const bool backlogged = queue.size() - 1 > targetBufferedFrames;
    if (!backlogged && now < queue.front().timeEstimate - lead) {
        return false;
    }

    // Once eligible, favor freshness over the newest frame's own deadline.
    // A backlog bypasses the gate so a stalled renderer can catch up.
    while (queue.size() > 1) {
        dropFrame(queue.front());
        queue.pop_front();
    }
    return true;
}

} // namespace artemis::streaming
