#include "../app/src/features/ui/QuickStreamMenu.hpp"

#include <algorithm>
#include <cassert>

using artemis::ui::QuickAction;
using artemis::ui::QuickMenuContext;
using artemis::ui::buildQuickActions;

int main() {
    auto actions = buildQuickActions({false, true});
    assert(std::find(actions.begin(), actions.end(), QuickAction::OpenMouseControls) != actions.end());
    assert(std::find(actions.begin(), actions.end(), QuickAction::ToggleTouchControls) != actions.end());
    assert(std::find(actions.begin(), actions.end(), QuickAction::StartBenchmark) != actions.end());
    assert(std::find(actions.begin(), actions.end(), QuickAction::StopBenchmark) == actions.end());
    assert(actions.back() == QuickAction::QuitHostApp);

    actions = buildQuickActions({true, false});
    assert(std::find(actions.begin(), actions.end(), QuickAction::StopBenchmark) != actions.end());
    assert(std::find(actions.begin(), actions.end(), QuickAction::QuitHostApp) == actions.end());
    return 0;
}
