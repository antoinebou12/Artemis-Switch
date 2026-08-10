#include "../app/src/features/ui/QuickStreamMenu.hpp"

#include <algorithm>
#include <cassert>

using artemis::ui::QuickAction;
using artemis::ui::buildQuickActions;

int main() {
    auto actions = buildQuickActions({true});
    assert(std::find(actions.begin(), actions.end(), QuickAction::ToggleKeyboard) != actions.end());
    assert(std::find(actions.begin(), actions.end(), QuickAction::TogglePerformanceStats) != actions.end());
    assert(std::find(actions.begin(), actions.end(), QuickAction::ToggleMouseMode) != actions.end());
    assert(std::find(actions.begin(), actions.end(), QuickAction::Disconnect) != actions.end());
    assert(actions.back() == QuickAction::QuitHostApp);

    actions = buildQuickActions({false});
    assert(std::find(actions.begin(), actions.end(), QuickAction::QuitHostApp) == actions.end());
    assert(actions.back() == QuickAction::Disconnect);
    return 0;
}
