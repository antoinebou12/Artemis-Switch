#include "HostAddressParse.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>

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

    unsigned a = 0;
    unsigned b = 0;
    unsigned c = 0;
    unsigned d = 0;
    char dot1 = 0;
    char dot2 = 0;
    char dot3 = 0;
    if (std::sscanf(host.c_str(), "%u%c%u%c%u%c%u", &a, &dot1, &b, &dot2, &c,
                    &dot3, &d) != 7) {
        return false;
    }
    if (dot1 != '.' || dot2 != '.' || dot3 != '.') {
        return false;
    }
    if (a > 255 || b > 255 || c > 255 || d > 255) {
        return false;
    }

    return a == 10 || a == 127 || (a == 169 && b == 254) ||
           (a == 192 && b == 168) || (a == 172 && b >= 16 && b <= 31);
}

bool parse_port(const std::string& text, unsigned short& out) {
    if (text.empty()) {
        return false;
    }
    for (char ch : text) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    const long value = std::strtol(text.c_str(), nullptr, 10);
    if (value <= 0 || value > 65535) {
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
