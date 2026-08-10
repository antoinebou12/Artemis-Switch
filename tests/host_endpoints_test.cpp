#include "../app/src/host/HostEndpoints.hpp"

#include <cassert>
#include <string>
#include <vector>

int main() {
    {
        const auto addresses = ordered_connection_addresses(
            {}, "192.168.1.10", "100.64.1.2");
        assert(addresses.size() == 2);
        assert(addresses[0] == "192.168.1.10");
        assert(addresses[1] == "100.64.1.2");
    }

    {
        std::vector<HostEndpoint> endpoints = {
            {"Tailscale", "100.64.1.2", 1},
            {"LAN", "192.168.1.10", 0},
            {"Public", "203.0.113.5", 2},
        };
        const auto addresses =
            ordered_connection_addresses(endpoints, "", "");
        assert(addresses.size() == 3);
        assert(addresses[0] == "192.168.1.10");
        assert(addresses[1] == "100.64.1.2");
        assert(addresses[2] == "203.0.113.5");
    }

    {
        std::vector<HostEndpoint> endpoints;
        std::string address = "192.168.1.10";
        std::string remote;
        add_host_endpoint(endpoints, address, remote, "Tailscale",
                          "100.64.1.2");
        assert(endpoints.size() == 2);
        assert(address == "192.168.1.10");
        assert(remote == "100.64.1.2");
    }

    return 0;
}
