#include "TailscaleWgxRoute.hpp"

namespace artemis::tailscale {

bool TailscaleWgxRoute::start(const RemoteRouteTarget& target,
                              std::string* error) {
    (void)target;
    if (error)
        *error =
            "Tailscale encrypted packet path is not release-ready; refusing "
            "to advertise a routable connection";
    return false;
}

bool TailscaleWgxRoute::prepareForStreaming(const RemoteRouteTarget& target,
                                            std::string* error) {
    (void)target;
    (void)error;
    return false;
}

void TailscaleWgxRoute::stop() noexcept {}

} // namespace artemis::tailscale