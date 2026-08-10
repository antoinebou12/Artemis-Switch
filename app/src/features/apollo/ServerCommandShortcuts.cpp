#include "ServerCommandShortcuts.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>

namespace artemis::apollo {
namespace {

std::string lowercase(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char ch : value)
        out.push_back(static_cast<char>(std::tolower(ch)));
    return out;
}

bool containsToken(std::string_view haystack, std::string_view needle) {
    return haystack.find(needle) != std::string_view::npos;
}

// Prefer more specific reset-display phrases over a bare "restart".
bool looksLikeResetDisplay(std::string_view lower) {
    if (containsToken(lower, "reset display device config") ||
        containsToken(lower, "reset display device") ||
        containsToken(lower, "reset-display-device") ||
        containsToken(lower, "reset_display_device") ||
        containsToken(lower, "reset display") ||
        containsToken(lower, "reset-display") ||
        containsToken(lower, "reset_display") ||
        containsToken(lower, "display device persistence") ||
        containsToken(lower, "display persistence") ||
        containsToken(lower, "reset vdisplay") ||
        containsToken(lower, "reset virtual display")) {
        return true;
    }

    if (containsToken(lower, "reset") &&
        (containsToken(lower, "display") || containsToken(lower, "monitor") ||
         containsToken(lower, "vdisplay") ||
         containsToken(lower, "virtual display"))) {
        return true;
    }
    return false;
}

bool looksLikeRestart(std::string_view lower) {
    if (looksLikeResetDisplay(lower))
        return false;

    if (lower == "restart" || lower == "reboot")
        return true;

    if (containsToken(lower, "restart server") ||
        containsToken(lower, "restart apollo") ||
        containsToken(lower, "restart sunshine") ||
        containsToken(lower, "restart host") ||
        containsToken(lower, "restart service") ||
        containsToken(lower, "reboot server") ||
        containsToken(lower, "reboot host")) {
        return true;
    }

    if ((containsToken(lower, "restart") || containsToken(lower, "reboot")) &&
        !containsToken(lower, "display") && !containsToken(lower, "monitor") &&
        !containsToken(lower, "vdisplay")) {
        return true;
    }
    return false;
}

std::optional<MatchedServerCommand>
matchKind(const std::vector<std::string>& commands,
          ServerCommandShortcutKind kind,
          bool (*predicate)(std::string_view)) {
    const std::size_t limit =
        std::min<std::size_t>(commands.size(),
                              static_cast<std::size_t>(UINT8_MAX) + 1);
    for (std::size_t i = 0; i < limit; ++i) {
        const auto lower = lowercase(commands[i]);
        if (!predicate(lower))
            continue;
        MatchedServerCommand match;
        match.kind = kind;
        match.index = i;
        match.advertisedName = commands[i];
        return match;
    }
    return std::nullopt;
}

} // namespace

bool serverCommandNameLooksLikeRestart(std::string_view name) {
    return looksLikeRestart(lowercase(name));
}

bool serverCommandNameLooksLikeResetDisplay(std::string_view name) {
    return looksLikeResetDisplay(lowercase(name));
}

std::optional<MatchedServerCommand>
findAdvertisedServerCommand(const std::vector<std::string>& commands,
                            ServerCommandShortcutKind kind) {
    switch (kind) {
    case ServerCommandShortcutKind::Restart:
        return matchKind(commands, kind, looksLikeRestart);
    case ServerCommandShortcutKind::ResetDisplay:
        return matchKind(commands, kind, looksLikeResetDisplay);
    }
    return std::nullopt;
}

std::vector<MatchedServerCommand>
findAdvertisedServerCommandShortcuts(
    const std::vector<std::string>& commands) {
    std::vector<MatchedServerCommand> matches;
    if (auto restart = findAdvertisedServerCommand(
            commands, ServerCommandShortcutKind::Restart))
        matches.push_back(std::move(*restart));
    if (auto reset = findAdvertisedServerCommand(
            commands, ServerCommandShortcutKind::ResetDisplay))
        matches.push_back(std::move(*reset));
    return matches;
}

} // namespace artemis::apollo
