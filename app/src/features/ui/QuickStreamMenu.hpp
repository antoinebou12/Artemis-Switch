#pragma once

#include <vector>

namespace artemis::ui {

enum class QuickAction {
    ToggleKeyboard,
    MoveActiveWindow,
    CycleDisplayMode,
    ShowConnectedControllers,
    OpenMouseControls,
    ToggleTouchControls,
    SendSpecialKey,
    StartBenchmark,
    StopBenchmark,
    Disconnect,
    QuitHostApp,
};

struct QuickMenuContext {
    bool benchmarkRunning = false;
    bool canQuitHostApp = true;
};

std::vector<QuickAction> buildQuickActions(const QuickMenuContext& context);

} // namespace artemis::ui
