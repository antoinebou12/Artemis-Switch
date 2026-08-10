#pragma once

#include <optional>
#include <string>

namespace artemis::host {

struct ParsedHostAddress {
    std::string host;
    std::optional<unsigned short> port;
    bool isIpv6 = false;
    bool isIpv4 = false;
    bool isHostname = false;
};

// Parses Moonlight host entry forms:
//   192.168.1.10
//   192.168.1.10:47989
//   stream.example.com
//   stream.example.com:47989
//   [2001:db8::1]
//   [2001:db8::1]:47989
ParsedHostAddress parse_host_address(const std::string& input);

// True when a manually entered address should be stored as remoteAddress
// (public IPv4 or any hostname / IPv6 literal).
bool should_store_as_remote(const ParsedHostAddress& parsed);

std::string format_host_address(const ParsedHostAddress& parsed);

} // namespace artemis::host
