#include "HostAddressParse.hpp"

#include <array>
#include <cctype>
#include <charconv>
#include <string_view>

namespace artemis::host {
namespace {

bool looks_like_ipv4(const std::string& host) {
    int dots = 0;
    int digits = 0;
    for (char ch : host) {
        if (ch == '.') {
            if (digits == 0) {
                return false;
            }
            dots++;
            digits = 0;
            continue;
        }
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
        digits++;
    }
    return dots == 3 && digits > 0;
}

bool is_private_ipv4_host(const std::string& host) {
    if (!looks_like_ipv4(host)) {
        return false;
    }

    std::array<unsigned, 4> octets{};
    std::string_view remaining(host);
    for (std::size_t index = 0; index < octets.size(); ++index) {
        const auto separator = remaining.find('.');
        const auto part = remaining.substr(0, separator);
        const auto* begin = part.data();
        const auto* end = begin + part.size();
        const auto [next, error] = std::from_chars(begin, end, octets[index]);
        if (error != std::errc{} || next != end || octets[index] > 255) {
            return false;
        }
        if (index + 1 < octets.size()) {
            if (separator == std::string_view::npos) {
                return false;
            }
            remaining.remove_prefix(separator + 1);
        } else if (separator != std::string_view::npos) {
            return false;
        }
    }

    const auto a = octets[0];
    const auto b = octets[1];
    return a == 10 || a == 127 || (a == 169 && b == 254) ||
           (a == 192 && b == 168) || (a == 172 && b >= 16 && b <= 31);
}

bool parse_port(const std::string& text, unsigned short& out) {
    if (text.empty()) {
        return false;
    }
    unsigned value = 0;
    const auto* begin = text.data();
    const auto* end = begin + text.size();
    const auto [next, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || next != end || value == 0 || value > 65535) {
        return false;
    }
    out = static_cast<unsigned short>(value);
    return true;
}

} // namespace

ParsedHostAddress parse_host_address(const std::string& input) {
    ParsedHostAddress result;
    std::string value = input;
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }

    if (value.empty()) {
        return result;
    }

    if (value.front() == '[') {
        const auto close = value.find(']');
        if (close == std::string::npos) {
            result.host = value;
            result.isHostname = true;
            return result;
        }

        result.host = value.substr(1, close - 1);
        result.isIpv6 = true;
        if (close + 1 < value.size() && value[close + 1] == ':') {
            unsigned short port = 0;
            if (parse_port(value.substr(close + 2), port)) {
                result.port = port;
            }
        }
        return result;
    }

    const auto colon = value.find(':');
    if (colon == std::string::npos) {
        result.host = value;
        result.isIpv4 = looks_like_ipv4(value);
        result.isHostname = !result.isIpv4;
        // Bare IPv6 without brackets still has multiple colons; handled below.
        return result;
    }

    if (value.find(':', colon + 1) != std::string::npos) {
        // Multiple colons → treat as bare IPv6 without port.
        result.host = value;
        result.isIpv6 = true;
        return result;
    }

    result.host = value.substr(0, colon);
    unsigned short port = 0;
    if (parse_port(value.substr(colon + 1), port)) {
        result.port = port;
    }
    result.isIpv4 = looks_like_ipv4(result.host);
    result.isHostname = !result.isIpv4;
    return result;
}

bool should_store_as_remote(const ParsedHostAddress& parsed) {
    if (parsed.host.empty()) {
        return false;
    }
    if (parsed.isHostname || parsed.isIpv6) {
        return true;
    }
    if (parsed.isIpv4) {
        return !is_private_ipv4_host(parsed.host);
    }
    return false;
}

std::string format_host_address(const ParsedHostAddress& parsed) {
    if (parsed.host.empty()) {
        return {};
    }

    std::string out;
    if (parsed.isIpv6) {
        out = "[" + parsed.host + "]";
    } else {
        out = parsed.host;
    }

    if (parsed.port.has_value()) {
        out += ":" + std::to_string(*parsed.port);
    }
    return out;
}

} // namespace artemis::host
