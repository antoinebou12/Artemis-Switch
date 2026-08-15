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

// Artemis process exit / language restart / HOME-sleep end of stream: always
// cancel the host session so Steam/Apollo is not left running headless.
// Overlay "Disconnect" still honors Settings::terminate_app_on_disconnect.
inline bool shouldTerminateHostOnApplicationExit() { return true; }

// Present only while the Limelight session is live. After a drop or start
// error, skip deko3d so teardown can LiStopConnection on the next UI frame.
inline bool shouldPresentStreamFrame(bool sessionTerminated,
                                     bool sessionActive) {
    return !sessionTerminated && sessionActive;
}

} // namespace artemis::streaming
