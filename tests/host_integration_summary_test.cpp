#include <cassert>

#include "../app/src/host/HostIntegrationSummary.hpp"

int main() {
    const artemis::host::HostIntegrationLabels labels{
        "Stream", "Refresh", "Display", "Commands"};

    assert(artemis::host::formatHostIntegrationSummary(
               "Apollo 1.0", true, true, true, true, labels) ==
           "Apollo 1.0 · Stream · Refresh · Display · Commands");
    assert(artemis::host::formatHostIntegrationSummary(
               "Sunshine", false, false, false, false, labels) ==
           "Sunshine · Stream");
    return 0;
}
