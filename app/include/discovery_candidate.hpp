#pragma once
#include <string>

struct DiscoveryCandidate {
    std::string targetAddress;
    std::string providerId;
    std::string peerId;
    std::string displayName;
    int priority = 0;

    bool isDirect() const {
        return providerId.empty();
    }
};
