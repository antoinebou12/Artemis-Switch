#include "TailscaleHttpUpgrade.hpp"

#include <array>
#include <cassert>
#include <string>

using namespace artemis::tailscale;

int main() {
    std::array<std::uint8_t, 101> initiation{};
    initiation[2] = 1;
    const auto request =
        buildTs2021UpgradeRequest("controlplane.tailscale.com", initiation);
    assert(request.starts_with("POST /ts2021 HTTP/1.1\r\n"));
    assert(request.find("X-Tailscale-Handshake: ") != std::string::npos);
    assert(request.ends_with("Content-Length: 0\r\n\r\n"));
    assert(buildTs2021UpgradeRequest("bad\r\nhost", initiation).empty());

    std::string error;
    assert(validateTs2021UpgradeResponse(
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Connection: Upgrade\r\n"
        "Upgrade: tailscale-control-protocol\r\n\r\n",
        &error));
    assert(!validateTs2021UpgradeResponse(
        "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n", &error));
    assert(!validateTs2021UpgradeResponse(
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Connection: Upgrade\r\nUpgrade: websocket\r\n\r\n",
        &error));
    return 0;
}
