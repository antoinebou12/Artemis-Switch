#include "TailscaleHttpUpgrade.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace artemis::tailscale {
namespace {
constexpr std::size_t kMaxHeaderSize = 16 * 1024;
constexpr std::string_view kUpgrade = "tailscale-control-protocol";
constexpr std::string_view kBase64 =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64(std::span<const std::uint8_t> input) {
    std::string output;
    output.reserve((input.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < input.size(); i += 3) {
        const std::uint32_t first = input[i];
        const std::uint32_t second = i + 1 < input.size() ? input[i + 1] : 0;
        const std::uint32_t third = i + 2 < input.size() ? input[i + 2] : 0;
        const auto value = first << 16U | second << 8U | third;
        output.push_back(kBase64[(value >> 18U) & 63U]);
        output.push_back(kBase64[(value >> 12U) & 63U]);
        output.push_back(i + 1 < input.size() ? kBase64[(value >> 6U) & 63U]
                                              : '=');
        output.push_back(i + 2 < input.size() ? kBase64[value & 63U] : '=');
    }
    return output;
}

std::string lower(std::string_view value) {
    std::string output(value);
    std::transform(output.begin(), output.end(), output.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return output;
}

std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.remove_prefix(1);
    while (!value.empty() &&
           (value.back() == ' ' || value.back() == '\t' ||
            value.back() == '\r'))
        value.remove_suffix(1);
    return value;
}
} // namespace

std::string buildTs2021UpgradeRequest(
    std::string_view host,
    std::span<const std::uint8_t> noiseInitiation) {
    if (host.empty() || host.find_first_of("\r\n") != std::string_view::npos ||
        noiseInitiation.size() != 101)
        return {};
    std::string request = "POST /ts2021 HTTP/1.1\r\nHost: ";
    request.append(host);
    request.append("\r\nUpgrade: tailscale-control-protocol\r\n"
                   "Connection: upgrade\r\nX-Tailscale-Handshake: ");
    request.append(base64(noiseInitiation));
    request.append("\r\nContent-Length: 0\r\n\r\n");
    return request;
}

bool validateTs2021UpgradeResponse(std::string_view header,
                                   std::string* error) {
    const auto reject = [error](std::string_view message) {
        if (error) *error = std::string(message);
        return false;
    };
    if (header.size() > kMaxHeaderSize || !header.ends_with("\r\n\r\n"))
        return reject("invalid or oversized TS2021 HTTP response");
    const auto firstEnd = header.find("\r\n");
    if (firstEnd == std::string_view::npos)
        return reject("missing TS2021 HTTP status");
    const auto status = header.substr(0, firstEnd);
    if (!status.starts_with("HTTP/1.1 101 ") && status != "HTTP/1.1 101")
        return reject("TS2021 HTTP upgrade was not accepted");

    bool sawUpgrade = false;
    bool sawConnection = false;
    std::size_t offset = firstEnd + 2;
    while (offset < header.size() - 2) {
        const auto end = header.find("\r\n", offset);
        if (end == std::string_view::npos || end == offset) break;
        const auto line = header.substr(offset, end - offset);
        const auto colon = line.find(':');
        if (colon == std::string_view::npos)
            return reject("malformed TS2021 HTTP header");
        const auto name = lower(trim(line.substr(0, colon)));
        const auto value = lower(trim(line.substr(colon + 1)));
        if (name == "upgrade" && value == kUpgrade) sawUpgrade = true;
        if (name == "connection" && value.find("upgrade") != std::string::npos)
            sawConnection = true;
        offset = end + 2;
    }
    if (!sawUpgrade || !sawConnection)
        return reject("TS2021 HTTP upgrade headers are missing");
    return true;
}

} // namespace artemis::tailscale
