#pragma once

#include <vector>

namespace artemis::ui {

enum class QuickAction {
    ToggleKeyboard,
    TogglePerformanceStats,
    ToggleMouseMode,
    Disconnect,
    QuitHostApp,
};

struct QuickMenuContext {
    bool canQuitHostApp = true;
};

std::vector<QuickAction> buildQuickActions(const QuickMenuContext& context);

} // namespace artemis::ui
