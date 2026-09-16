#include "LowLatencyFrameSelection.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace std::chrono_literals;
using artemis::streaming::prepareLatestFrame;

namespace {
struct Frame {
    std::chrono::steady_clock::time_point timeEstimate;
    int id;
};

void check(bool condition, const char* message) {
    // Keep regression checks active in Release CI builds too.
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

void checkCadence(std::chrono::nanoseconds interval) {
    const auto start = std::chrono::steady_clock::time_point{};
    constexpr auto lead = 3ms;
    std::deque<Frame> queue;
    std::vector<int> dropped;
    auto recycle = [&](Frame& frame) { dropped.push_back(frame.id); };

    check(!prepareLatestFrame(queue, start, lead, 1, recycle), "empty queue");
    queue.push_back({start + interval, 1});
    const auto deadline = start + interval - lead;
    check(!prepareLatestFrame(queue, deadline - 1ns, lead, 1, recycle),
          "single frame must wait before its deadline");

    // A second arrival must not discard the held frame or move its gate.
    queue.push_back({start + interval * 2, 2});
    for (int attempt = 0; attempt < 3; ++attempt) {
        check(!prepareLatestFrame(queue, deadline - 1ns, lead, 1, recycle),
              "two frames must keep the oldest gate");
        check(queue.size() == 2 && queue.front().id == 1 && dropped.empty(),
              "holds must not recycle frames");
    }
    check(prepareLatestFrame(queue, deadline, lead, 1, recycle),
          "oldest deadline must release a frame at equality");
    check(queue.size() == 1 && queue.front().id == 2 && dropped == std::vector<int>{1},
          "release must keep newest and recycle oldest exactly once");
    queue.pop_front(); // renderer consumes the selected frame

    // Presentation continues after the held pair instead of starving.
    queue.push_back({start + interval * 3, 3});
    check(prepareLatestFrame(queue, start + interval * 3 - lead, lead, 1, recycle),
          "next frame must make progress");
    check(queue.front().id == 3 && dropped.size() == 1, "single release must not drop");
}
} // namespace

int main() {
    checkCadence(16666667ns); // 60 fps
    checkCadence(33333333ns); // 30 fps

    const auto now = std::chrono::steady_clock::time_point{};
    std::vector<int> dropped;
    auto recycle = [&](Frame& frame) { dropped.push_back(frame.id); };
    std::deque<Frame> queue{{now + 100ms, 1}, {now + 116ms, 2}, {now + 132ms, 3}};
    check(prepareLatestFrame(queue, now, 3ms, 1, recycle),
          "three-frame backlog must bypass even a future deadline");
    check(queue.size() == 1 && queue.front().id == 3 && dropped == std::vector<int>({1, 2}),
          "backlog must recycle in order and retain newest");

    queue = {{now + 100ms, 4}, {now + 116ms, 5}};
    dropped.clear();
    check(prepareLatestFrame(queue, now, 3ms, 0, recycle),
          "zero buffering target must bypass with two frames");
    check(queue.front().id == 5 && dropped == std::vector<int>{4}, "depth-zero selection");

    queue = {{now + 100ms, 6}};
    check(!prepareLatestFrame(queue, now, 3ms, 0, recycle),
          "one frame is not a backlog even with zero buffering target");
    check(prepareLatestFrame(queue, now + 1s, 3ms, 0, recycle),
          "renderer must recover after a long stall");
    check(queue.front().id == 6 && dropped == std::vector<int>{4}, "stall preserves ownership");
}
