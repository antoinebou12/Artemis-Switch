#include "HostAddressParse.hpp"

#include <cassert>
#include <iostream>

int main() {
    using artemis::host::parse_host_address;
    using artemis::host::should_store_as_remote;
    using artemis::host::format_host_address;

    {
        auto parsed = parse_host_address("192.168.1.10:47989");
        assert(parsed.host == "192.168.1.10");
        assert(parsed.port.has_value() && *parsed.port == 47989);
        assert(parsed.isIpv4);
        assert(!should_store_as_remote(parsed));
    }

    {
        auto parsed = parse_host_address("stream.example.com:47989");
        assert(parsed.host == "stream.example.com");
        assert(parsed.port.has_value() && *parsed.port == 47989);
        assert(parsed.isHostname);
        assert(should_store_as_remote(parsed));
        assert(format_host_address(parsed) == "stream.example.com:47989");
    }

    {
        auto parsed = parse_host_address("pc.lan");
        assert(parsed.host == "pc.lan");
        assert(!parsed.port.has_value());
        assert(parsed.isHostname);
        assert(should_store_as_remote(parsed));
    }

    {
        auto parsed = parse_host_address("[2001:db8::1]:47989");
        assert(parsed.host == "2001:db8::1");
        assert(parsed.isIpv6);
        assert(parsed.port.has_value() && *parsed.port == 47989);
        assert(should_store_as_remote(parsed));
        assert(format_host_address(parsed) == "[2001:db8::1]:47989");
    }

    {
        auto parsed = parse_host_address("8.8.8.8:47989");
        assert(parsed.isIpv4);
        assert(should_store_as_remote(parsed));
    }

    std::cout << "host_address_parse_test ok\n";
    return 0;
}
