#include "../app/src/features/apollo/ServerCommandShortcuts.hpp"

#include <cassert>
#include <string>
#include <vector>

using namespace artemis::apollo;

int main() {
    assert(serverCommandNameLooksLikeRestart("Restart"));
    assert(serverCommandNameLooksLikeRestart("restart apollo"));
    assert(serverCommandNameLooksLikeRestart("Restart Server"));
    assert(serverCommandNameLooksLikeRestart("Reboot host"));
    assert(!serverCommandNameLooksLikeRestart("Toggle OSD"));
    assert(!serverCommandNameLooksLikeRestart("Reset Display Device Config"));

    assert(serverCommandNameLooksLikeResetDisplay(
        "Reset Display Device Config"));
    assert(serverCommandNameLooksLikeResetDisplay("reset display"));
    assert(serverCommandNameLooksLikeResetDisplay("Reset virtual display"));
    assert(serverCommandNameLooksLikeResetDisplay(
        "reset-display-device-persistence"));
    assert(!serverCommandNameLooksLikeResetDisplay("Restart"));
    assert(!serverCommandNameLooksLikeResetDisplay("Toggle OSD"));

    const std::vector<std::string> commands = {
        "Toggle OSD",
        "Restart",
        "Reset Display Device Config",
        "Custom backup",
    };

    const auto restart = findAdvertisedServerCommand(
        commands, ServerCommandShortcutKind::Restart);
    assert(restart.has_value());
    assert(restart->index == 1);
    assert(restart->advertisedName == "Restart");

    const auto reset = findAdvertisedServerCommand(
        commands, ServerCommandShortcutKind::ResetDisplay);
    assert(reset.has_value());
    assert(reset->index == 2);
    assert(reset->advertisedName == "Reset Display Device Config");

    const auto shortcuts = findAdvertisedServerCommandShortcuts(commands);
    assert(shortcuts.size() == 2);
    assert(shortcuts[0].kind == ServerCommandShortcutKind::Restart);
    assert(shortcuts[1].kind == ServerCommandShortcutKind::ResetDisplay);

    const std::vector<std::string> genericOnly = {"Toggle OSD", "Mute mic"};
    assert(!findAdvertisedServerCommand(genericOnly,
                                        ServerCommandShortcutKind::Restart));
    assert(!findAdvertisedServerCommand(
        genericOnly, ServerCommandShortcutKind::ResetDisplay));
    assert(findAdvertisedServerCommandShortcuts(genericOnly).empty());

    const std::vector<std::string> restartVariants = {
        "Restart Sunshine",
        "restart apollo service",
    };
    const auto firstRestart = findAdvertisedServerCommand(
        restartVariants, ServerCommandShortcutKind::Restart);
    assert(firstRestart.has_value());
    assert(firstRestart->index == 0);

    return 0;
}
