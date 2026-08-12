#include "StreamDisconnectPolicy.hpp"
#include "StreamUiLifecycle.hpp"

#include <cassert>

using namespace artemis::streaming;

int main() {
    assert(connectionTerminatedAction(true, false, 0) ==
           ConnectionTerminatedAction::MarkTerminated);
    assert(connectionTerminatedAction(true, false, 1) ==
           ConnectionTerminatedAction::MarkTerminated);
    assert(connectionTerminatedAction(false, false, 0) ==
           ConnectionTerminatedAction::MarkTerminated);
    assert(connectionTerminatedAction(false, false, -1) ==
           ConnectionTerminatedAction::MarkTerminated);
    assert(connectionTerminatedAction(false, true, 0) ==
           ConnectionTerminatedAction::MarkInactiveForRestart);
    assert(connectionTerminatedAction(false, true, 1) ==
           ConnectionTerminatedAction::MarkInactiveForRestart);
    assert(connectionTerminatedAction(true, true, 0) ==
           ConnectionTerminatedAction::MarkTerminated);

    assert(shouldReloadAppsAfterHostQuit(true));
    assert(shouldReloadAppsAfterHostQuit(false));

    markStreamUiClosed();
    assert(consumeStreamUiClosed());
    assert(!consumeStreamUiClosed());

    return 0;
}
