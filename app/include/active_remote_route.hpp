#pragma once
#include <string>

struct ActiveRemoteRoute {
    std::string providerId;
    std::string peerId;
    std::string targetAddress;
    std::string connectAddress;
};
