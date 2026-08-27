#pragma once

#include <string>
#include <vector>

namespace artemis::host {

struct HostIntegrationLabels {
    std::string stream;
    std::string refresh;
    std::string display;
    std::string commands;
};

inline std::string formatHostIntegrationSummary(
    const std::string& product, bool preciseRefreshRate,
    bool clientVirtualDisplay, bool hostVirtualDisplay, bool serverCommands,
    const HostIntegrationLabels& labels) {
    std::vector<std::string> features;
    features.reserve(4);
    features.push_back(labels.stream);
    if (preciseRefreshRate)
        features.push_back(labels.refresh);
    if (clientVirtualDisplay || hostVirtualDisplay)
        features.push_back(labels.display);
    if (serverCommands)
        features.push_back(labels.commands);

    std::string detail = product;
    for (const auto& feature : features)
        detail += " · " + feature;
    return detail;
}

} // namespace artemis::host
