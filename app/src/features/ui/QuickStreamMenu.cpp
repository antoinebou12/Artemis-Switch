#include "QuickStreamMenu.hpp"

namespace artemis::ui {

std::vector<QuickAction> buildQuickActions(const QuickMenuContext& context) {
    std::vector<QuickAction> actions = {
        QuickAction::OpenMouseControls,
        QuickAction::ToggleKeyboard,
        QuickAction::MoveActiveWindow,
        QuickAction::CycleDisplayMode,
        QuickAction::ShowConnectedControllers,
        QuickAction::ToggleTouchControls,
        QuickAction::SendSpecialKey,
        context.benchmarkRunning ? QuickAction::StopBenchmark
                                 : QuickAction::StartBenchmark,
        QuickAction::Disconnect,
    };

    if (context.canQuitHostApp)
        actions.push_back(QuickAction::QuitHostApp);

    return actions;
}

} // namespace artemis::ui
