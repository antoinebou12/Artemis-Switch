#pragma once

#include <string>
#include <string_view>

namespace artemis::apollo {

enum class VirtualDisplayTarget {
    Off,
    CurrentProfile,
    Handheld,
    Docked,
    PortraitHandheld,
    PortraitDocked,
    Custom,
};

struct ApolloHostOptions {
    VirtualDisplayTarget target = VirtualDisplayTarget::Off;
    int customWidth = 1280;
    int customHeight = 720;
    int refreshRate = 60;
};

struct ResolvedVirtualDisplay {
    bool enabled = false;
    int width = 0;
    int height = 0;
    int refreshRate = 0;
};

ApolloHostOptions validateApolloHostOptions(ApolloHostOptions options);
ResolvedVirtualDisplay resolveVirtualDisplay(const ApolloHostOptions& options,
                                             int profileWidth,
                                             int profileHeight);
const char* virtualDisplayTargetName(VirtualDisplayTarget target);
bool virtualDisplayTargetFromName(std::string_view name,
                                  VirtualDisplayTarget& target);
bool parseVirtualDisplaySpec(std::string_view text, ApolloHostOptions& options,
                             std::string* error = nullptr);

} // namespace artemis::apollo
