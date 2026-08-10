#include "QuickStreamMenu.hpp"

namespace artemis::ui {

std::vector<QuickAction> buildQuickActions(const QuickMenuContext& context) {
    std::vector<QuickAction> actions = {
        QuickAction::ToggleKeyboard,
        QuickAction::TogglePerformanceStats,
        QuickAction::ToggleMouseMode,
        QuickAction::Disconnect,
    };

    if (context.canQuitHostApp)
        actions.push_back(QuickAction::QuitHostApp);

    return actions;
}

} // namespace artemis::ui
