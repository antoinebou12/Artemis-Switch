#include "../app/src/features/input/ControllerTopology.hpp"

#include <cassert>

using namespace artemis::input;

int main() {
    assert(clampControllerCount(-1) == 0);
    assert(clampControllerCount(0) == 0);
    assert(clampControllerCount(3) == 3);
    assert(clampControllerCount(5) == 5);
    assert(clampControllerCount(9) == MaxSupportedControllers);

    assert(connectedControllerMask(0) == 0x00);
    assert(connectedControllerMask(1) == 0x01);
    assert(connectedControllerMask(2) == 0x03);
    assert(connectedControllerMask(4) == 0x0F);
    assert(connectedControllerMask(5) == 0x1F);
    assert(connectedControllerMask(20) == 0x1F);

    assert(launchControllerMask(0) == 0x01);
    assert(launchControllerMask(1) == 0x01);
    assert(launchControllerMask(2) == 0x03);
    assert(launchControllerMask(5) == 0x1F);

    const auto players = connectedControllerPlayers(5);
    assert(players.size() == 5);
    assert(players.front() == 1);
    assert(players.back() == 5);
    assert(connectedControllerPlayers(0).empty());
    return 0;
}
