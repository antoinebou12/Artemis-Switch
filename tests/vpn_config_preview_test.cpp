#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "../app/src/remote_access/VpnConfigPreview.hpp"

using artemis::remote_access::VpnConfigLoadStatus;

int main() {
    const std::string wireGuard =
        "# keep this comment\n"
        "# token=example-in-comment\n"
        "[Interface]\n"
        "PrivateKey = wireguard-private # generated locally\n"
        "Address = 10.0.0.2/32\n"
        "[Peer]\n"
        "PublicKey = safe-public-value\n"
        "PresharedKey = shared-secret-value\n";
    const auto redactedWireGuard =
        artemis::remote_access::redactVpnConfig(wireGuard);
    assert(redactedWireGuard.find("wireguard-private") == std::string::npos);
    assert(redactedWireGuard.find("shared-secret-value") == std::string::npos);
    assert(redactedWireGuard.find("[REDACTED]") != std::string::npos);
    assert(redactedWireGuard.find("# keep this comment") != std::string::npos);
    assert(redactedWireGuard.find("# token=example-in-comment") !=
           std::string::npos);
    assert(redactedWireGuard.find("# generated locally") != std::string::npos);
    assert(redactedWireGuard.find("safe-public-value") != std::string::npos);
    assert(redactedWireGuard.find("10.0.0.2/32") != std::string::npos);

    const std::string mixed =
        R"({"AuthToken":"never-show","name":"kept","nested_secret":"also-hide"})"
        "\nSeTuP_KeY = setup-hide\n"
        "api-key: api-hide\n"
        "PASSWORD = password-hide\n";
    const auto redactedMixed =
        artemis::remote_access::redactVpnConfig(mixed);
    for (const auto* secret : {"never-show", "also-hide", "setup-hide",
                               "api-hide", "password-hide"}) {
        assert(redactedMixed.find(secret) == std::string::npos);
    }
    assert(redactedMixed.find("\"name\":\"kept\"") != std::string::npos);

    const auto root = std::filesystem::temp_directory_path() /
                      "artemis-vpn-config-preview-test";
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    assert(!ec);

    const auto normalPath = root / "config.conf";
    {
        std::ofstream out(normalPath, std::ios::binary);
        out << wireGuard;
    }
    auto preview = artemis::remote_access::loadVpnConfigPreview(
        normalPath.string());
    assert(preview.status == VpnConfigLoadStatus::Ok);
    assert(!preview.truncated);
    assert(preview.text.find("wireguard-private") == std::string::npos);

    const auto emptyPath = root / "empty.conf";
    { std::ofstream out(emptyPath, std::ios::binary); }
    preview = artemis::remote_access::loadVpnConfigPreview(emptyPath.string());
    assert(preview.status == VpnConfigLoadStatus::Empty);

    preview = artemis::remote_access::loadVpnConfigPreview(
        (root / "missing.conf").string());
    assert(preview.status == VpnConfigLoadStatus::Missing);

    const auto largePath = root / "large.json";
    {
        std::ofstream out(largePath, std::ios::binary);
        out << "token=must-hide\n" << std::string(256, 'x');
    }
    preview = artemis::remote_access::loadVpnConfigPreview(
        largePath.string(), 32);
    assert(preview.status == VpnConfigLoadStatus::Ok);
    assert(preview.truncated);
    assert(preview.text.find("must-hide") == std::string::npos);

    std::filesystem::remove_all(root, ec);
    return 0;
}
