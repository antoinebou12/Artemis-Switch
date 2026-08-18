#pragma once

#include <cstdint>
#include <vector>

namespace artemis::input {

constexpr int MaxSupportedControllers = 5;

int clampControllerCount(int reportedCount);
std::uint16_t connectedControllerMask(int reportedCount);
std::uint16_t launchControllerMask(int reportedCount);
std::vector<int> connectedControllerPlayers(int reportedCount);

} // namespace artemis::input
