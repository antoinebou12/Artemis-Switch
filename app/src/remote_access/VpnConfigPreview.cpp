#include "VpnConfigPreview.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>

namespace artemis::remote_access {
namespace {

std::string normalizeKey(std::string_view key) {
    std::string normalized;
    normalized.reserve(key.size());
    for (const unsigned char ch : key) {
        if (std::isalnum(ch))
            normalized.push_back(static_cast<char>(std::tolower(ch)));
    }
    return normalized;
}

bool isSensitiveKey(std::string_view key) {
    const auto normalized = normalizeKey(key);
    if (normalized.empty())
        return false;

    // Match the common exact key forms and vendor-prefixed variants. Public
    // keys and endpoints deliberately do not match.
    static constexpr std::string_view exact[] = {
        "privatekey", "presharedkey", "setupkey", "authkey", "apikey",
        "apiaccesskey", "clientsecret", "password", "passwd", "token",
        "secret",
    };
    if (std::find(std::begin(exact), std::end(exact), normalized) !=
        std::end(exact)) {
        return true;
    }
    return normalized.find("password") != std::string::npos ||
           normalized.find("passwd") != std::string::npos ||
           normalized.find("token") != std::string::npos ||
           normalized.find("secret") != std::string::npos ||
           normalized.ends_with("privatekey") ||
           normalized.ends_with("presharedkey") ||
           normalized.ends_with("setupkey") ||
           normalized.ends_with("authkey") ||
           normalized.ends_with("apikey");
}

bool onlyWhitespace(std::string_view text) {
    return std::all_of(text.begin(), text.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
}

std::string redactAssignments(std::string_view source,
                              const std::regex& assignment) {
    const std::string input(source);
    std::string output;
    output.reserve(input.size());
    std::size_t consumed = 0;

    for (std::sregex_iterator it(input.begin(), input.end(), assignment), end;
         it != end; ++it) {
        const auto& match = *it;
        const std::string key = match[2].matched ? match[2].str()
                                                 : match[3].str();
        if (!isSensitiveKey(key))
            continue;

        const auto valueStart =
            static_cast<std::size_t>(match.position(5));
        const auto valueLength = static_cast<std::size_t>(match.length(5));
        output.append(input, consumed, valueStart - consumed);

        const auto originalValue = input.substr(valueStart, valueLength);
        const bool quotedValue = !originalValue.empty() &&
                                 originalValue.front() == '"';
        const bool quotedKey = match[2].matched;
        output.append(quotedValue || quotedKey ? "\"[REDACTED]\""
                                               : "[REDACTED]");

        // Keep trailing whitespace and key/value inline comments. The value is
        // removed before copying either suffix, so the comment cannot expose it.
        std::size_t suffix = originalValue.size();
        if (!quotedValue) {
            for (const auto marker : {std::string_view(" #"),
                                      std::string_view(" ;")}) {
                const auto found = originalValue.find(marker);
                if (found != std::string::npos)
                    suffix = std::min(suffix, found);
            }
        }
        if (suffix == originalValue.size()) {
            while (suffix > 0 &&
                   std::isspace(static_cast<unsigned char>(
                       originalValue[suffix - 1]))) {
                --suffix;
            }
        }
        output.append(originalValue.substr(suffix));
        consumed = valueStart + valueLength;
    }

    output.append(input, consumed, std::string::npos);
    return output;
}

} // namespace

std::string redactVpnConfig(std::string_view config) {
    // Covers INI/WireGuard key=value, NetBird-style key/value files, and both
    // compact and pretty-printed JSON-like objects while retaining surrounding
    // syntax and line formatting.
    static const std::regex assignment(
        R"REGEX(("([^"]+)"|([A-Za-z_][A-Za-z0-9_.-]*))(\s*[:=]\s*)("(?:\\.|[^"\\])*"|[^,\r\n}]+))REGEX",
        std::regex::ECMAScript);

    std::string output;
    output.reserve(config.size());
    std::size_t offset = 0;
    while (offset < config.size()) {
        const auto newline = config.find('\n', offset);
        const auto lineEnd = newline == std::string_view::npos
                                 ? config.size()
                                 : newline;
        const auto line = config.substr(offset, lineEnd - offset);
        const auto first = line.find_first_not_of(" \t\r");
        if (first != std::string_view::npos &&
            (line[first] == '#' || line[first] == ';')) {
            output.append(line);
        } else {
            output.append(redactAssignments(line, assignment));
        }
        if (newline != std::string_view::npos)
            output.push_back('\n');
        offset = lineEnd + (newline == std::string_view::npos ? 0 : 1);
    }
    return output;
}

VpnConfigPreview loadVpnConfigPreview(const std::string& path,
                                      std::size_t displayLimit) {
    VpnConfigPreview result;
    if (path.empty())
        return result;

    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec)
        return result;

    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        result.status = VpnConfigLoadStatus::Unreadable;
        return result;
    }

    std::string bytes(displayLimit + 1, '\0');
    stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    const auto read = static_cast<std::size_t>(stream.gcount());
    if (stream.bad()) {
        result.status = VpnConfigLoadStatus::Unreadable;
        return result;
    }

    bytes.resize(read);
    if (bytes.empty() || onlyWhitespace(bytes)) {
        result.status = VpnConfigLoadStatus::Empty;
        return result;
    }

    result.truncated = bytes.size() > displayLimit;
    if (result.truncated)
        bytes.resize(displayLimit);
    result.text = redactVpnConfig(bytes);
    result.status = VpnConfigLoadStatus::Ok;
    return result;
}

} // namespace artemis::remote_access
