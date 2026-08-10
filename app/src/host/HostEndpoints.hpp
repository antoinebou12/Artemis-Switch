#pragma once

#include <algorithm>
#include <string>
#include <vector>

struct HostEndpoint {
    std::string label;
    std::string address;
    int priority = 0;
};

inline std::vector<HostEndpoint> endpoints_or_legacy(
    const std::vector<HostEndpoint>& endpoints, const std::string& address,
    const std::string& remoteAddress) {
    if (!endpoints.empty()) {
        return endpoints;
    }

    std::vector<HostEndpoint> legacy;
    if (!address.empty()) {
        legacy.push_back({"Local", address, 0});
    }
    if (!remoteAddress.empty() && remoteAddress != address) {
        legacy.push_back({"Remote", remoteAddress, 1});
    }
    return legacy;
}

inline std::vector<std::string> ordered_connection_addresses(
    const std::vector<HostEndpoint>& endpoints, const std::string& address,
    const std::string& remoteAddress) {
    auto eps = endpoints_or_legacy(endpoints, address, remoteAddress);
    std::stable_sort(eps.begin(), eps.end(),
                     [](const HostEndpoint& a, const HostEndpoint& b) {
                         return a.priority < b.priority;
                     });

    std::vector<std::string> addresses;
    for (const auto& endpoint : eps) {
        if (endpoint.address.empty()) {
            continue;
        }
        if (std::find(addresses.begin(), addresses.end(), endpoint.address) ==
            addresses.end()) {
            addresses.push_back(endpoint.address);
        }
    }
    return addresses;
}

inline void sync_legacy_fields_from_endpoints(
    const std::vector<HostEndpoint>& endpoints, std::string& address,
    std::string& remoteAddress) {
    const auto addresses =
        ordered_connection_addresses(endpoints, address, remoteAddress);
    address = addresses.empty() ? "" : addresses.front();
    remoteAddress = addresses.size() > 1 ? addresses[1] : "";
}

inline void add_host_endpoint(std::vector<HostEndpoint>& endpoints,
                              std::string& address, std::string& remoteAddress,
                              const std::string& label,
                              const std::string& endpointAddress) {
    if (endpointAddress.empty()) {
        return;
    }

    endpoints = endpoints_or_legacy(endpoints, address, remoteAddress);
    for (const auto& endpoint : endpoints) {
        if (endpoint.address == endpointAddress) {
            sync_legacy_fields_from_endpoints(endpoints, address, remoteAddress);
            return;
        }
    }

    int nextPriority = 0;
    for (const auto& endpoint : endpoints) {
        nextPriority = std::max(nextPriority, endpoint.priority + 1);
    }
    endpoints.push_back({label.empty() ? "Custom" : label, endpointAddress,
                         nextPriority});
    sync_legacy_fields_from_endpoints(endpoints, address, remoteAddress);
}
