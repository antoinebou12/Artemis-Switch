#pragma once

namespace artemis::streaming {

// What the Limelight connectionTerminated callback may do.
// GPU teardown (LiStopConnection) and reconnect must never run on that
// worker thread — they race deko3d and orange-screen the Switch.
enum class ConnectionTerminatedAction {
    // Host closed / app deleted / drop. UI thread tears the session down.
    MarkTerminated,
    // Planned restart: the old socket died with error 0. Keep the session.
    MarkInactiveForRestart,
};

inline ConnectionTerminatedAction connectionTerminatedAction(
    bool stopRequested, bool restartInProgress, int /*errorCode*/) {
    if (!stopRequested && restartInProgress)
        return ConnectionTerminatedAction::MarkInactiveForRestart;
    return ConnectionTerminatedAction::MarkTerminated;
}

// Quit can fail after the host already deleted/closed the app. Still reload.
inline bool shouldReloadAppsAfterHostQuit(bool /*quitSucceeded*/) {
    return true;
}

} // namespace artemis::streaming
