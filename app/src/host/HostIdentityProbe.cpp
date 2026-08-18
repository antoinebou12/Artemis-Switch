#include "HostIdentityProbe.hpp"

#include "../features/host/HostAddressParse.hpp"
#include "../libgamestream/errors.h"
#include "../libgamestream/http.h"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace artemis::host {
namespace {

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return value;
}

std::optional<std::string> jsonString(const std::string& body,
                                      const std::string& key) {
    const std::string marker = "\"" + key + "\"";
    auto pos = body.find(marker);
    if (pos == std::string::npos)
        return std::nullopt;
    pos = body.find(':', pos + marker.size());
    if (pos == std::string::npos)
        return std::nullopt;
    pos = body.find_first_not_of(" \t\r\n", pos + 1);
    if (pos == std::string::npos || body[pos] != '"')
        return std::nullopt;
    const auto end = body.find('"', pos + 1);
    if (end == std::string::npos)
        return std::nullopt;
    return body.substr(pos + 1, end - pos - 1);
}

bool jsonNonNegativeInteger(const std::string& body, const std::string& key) {
    const std::string marker = "\"" + key + "\"";
    auto pos = body.find(marker);
    if (pos == std::string::npos)
        return false;
    pos = body.find(':', pos + marker.size());
    if (pos == std::string::npos)
        return false;
    pos = body.find_first_not_of(" \t\r\n", pos + 1);
    if (pos == std::string::npos || !std::isdigit(
            static_cast<unsigned char>(body[pos])))
        return false;
    while (pos < body.size() && std::isdigit(
               static_cast<unsigned char>(body[pos])))
        ++pos;
    pos = body.find_first_not_of(" \t\r\n", pos);
    return pos < body.size() && (body[pos] == ',' || body[pos] == '}');
}

std::string_view trimmed(std::string_view value) {
    while (!value.empty() && std::isspace(
               static_cast<unsigned char>(value.front())))
        value.remove_prefix(1);
    while (!value.empty() && std::isspace(
               static_cast<unsigned char>(value.back())))
        value.remove_suffix(1);
    return value;
}

std::string urlHost(const std::string& address) {
    auto parsed = parse_host_address(address);
    parsed.port.reset();
    return format_host_address(parsed);
}

std::optional<std::string> getProbe(const std::string& url) {
    Data data;
    HTTPRequestOptions options;
    options.maxResponseBytes = 64 * 1024;
    options.connectTimeoutMs = 350;
    options.totalTimeoutMs = 750;
    options.suppressErrors = true;
    HTTPResponseInfo response;
    const int result =
        http_request(url, &data, HTTPRequestTimeoutLow, options, &response);
    if (result != GS_OK ||
        response.status != 200) {
        return std::nullopt;
    }
    return std::string(reinterpret_cast<const char*>(data.bytes()), data.size());
}

} // namespace

std::optional<HostIdentity> parsePunktfunkHealth(const std::string& body) {
    const auto object = trimmed(body);
    if (object.size() < 2 || object.front() != '{' || object.back() != '}')
        return std::nullopt;

    const auto status = jsonString(body, "status");
    const auto version = jsonString(body, "version");
    if (!status || lowercase(*status) != "ok" || !version || version->empty() ||
        !jsonNonNegativeInteger(body, "abi_version")) {
        return std::nullopt;
    }

    auto identity = HostCapabilityPolicy::identityFor(HostKind::Punktfunk);
    identity.version = *version;
    return identity;
}

std::optional<HostIdentity> parseVibeshineWebUi(const std::string& body) {
    const std::string normalized = lowercase(body);
    if (normalized.find("<title>vibeshine</title>") == std::string::npos &&
        normalized.find("content=\"vibeshine\"") == std::string::npos &&
        normalized.find("content='vibeshine'") == std::string::npos) {
        return std::nullopt;
    }
    return HostCapabilityPolicy::identityFor(HostKind::Vibeshine);
}

std::optional<HostIdentity> probePunktfunkIdentity(
    const std::string& address) {
    const auto host = urlHost(address);
    if (host.empty())
        return std::nullopt;
    const auto body = getProbe("https://" + host +
                               ":47992/api/v1/health");
    return body ? parsePunktfunkHealth(*body) : std::nullopt;
}

std::optional<HostIdentity> probeHostIdentity(
    const std::string& address, bool virtualDisplayHint) {
    const auto host = urlHost(address);
    if (host.empty())
        return std::nullopt;

    if (virtualDisplayHint) {
        const auto body = getProbe("https://" + host + ":47990/");
        if (body) {
            if (auto vibeshine = parseVibeshineWebUi(*body))
                return vibeshine;
        }
    }
    return probePunktfunkIdentity(address);
}

std::string hostConsoleUrl(const std::string& address,
                           const HostIdentity& identity) {
    const auto host = urlHost(address);
    if (host.empty())
        return {};
    const unsigned short port = identity.webConsolePort != 0
        ? identity.webConsolePort
        : 47990;
    return "https://" + host + ":" + std::to_string(port) + "/";
}

const char* punktfunkGameStreamRequiredError() {
    return "Punktfunk detected, but its Moonlight/GameStream compatibility "
           "plane is disabled. Enable GameStream on the host, then reconnect.";
}

} // namespace artemis::host
