#include "ControllerTopology.hpp"

#include <algorithm>

namespace artemis::input {

int clampControllerCount(int reportedCount) {
    return std::clamp(reportedCount, 0, MaxSupportedControllers);
}

std::uint16_t connectedControllerMask(int reportedCount) {
    const int count = clampControllerCount(reportedCount);
    return count == 0
               ? 0
               : static_cast<std::uint16_t>((1u << count) - 1u);
}

std::vector<int> connectedControllerPlayers(int reportedCount) {
    const int count = clampControllerCount(reportedCount);
    std::vector<int> players;
    players.reserve(count);
    for (int player = 1; player <= count; ++player)
        players.push_back(player);
    return players;
}

} // namespace artemis::input
