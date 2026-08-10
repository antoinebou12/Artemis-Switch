#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace artemis::apollo {

enum class ServerCommandShortcutKind {
    Restart,
    ResetDisplay,
};

struct MatchedServerCommand {
    ServerCommandShortcutKind kind;
    std::size_t index = 0;
    std::string advertisedName;
};

// Map labeled Quick shortcuts onto host-advertised ServerCommand names only.
// Matching is case-insensitive substring against common restart / reset
// display strings. No GameStream endpoints are invented.
std::optional<MatchedServerCommand>
findAdvertisedServerCommand(const std::vector<std::string>& commands,
                            ServerCommandShortcutKind kind);

std::vector<MatchedServerCommand>
findAdvertisedServerCommandShortcuts(
    const std::vector<std::string>& commands);

bool serverCommandNameLooksLikeRestart(std::string_view name);
bool serverCommandNameLooksLikeResetDisplay(std::string_view name);

} // namespace artemis::apollo
