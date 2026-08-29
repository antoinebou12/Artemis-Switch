#include "TailscaleAuthKeyFile.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

using namespace artemis::tailscale;

int main() {
    const auto root = std::filesystem::temp_directory_path() /
                      "artemis-tailscale-auth-key-test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root / "tailscale");

    assert(defaultAuthKeyPath(root.string()) ==
           (root / "tailscale" / "auth.key").generic_string());

    {
        std::ofstream key(root / "tailscale" / "auth-key.txt");
        key << "# one-off key\n"
               "auth_key = tskey-auth-test-value-123456789\n";
    }
    auto discovered = loadAuthKeyFile(root.string(), {});
    assert(discovered.found());
    assert(discovered.key == "tskey-auth-test-value-123456789");
    assert(discovered.path ==
           (root / "tailscale" / "auth-key.txt").generic_string());

    const auto selected = root / "chosen.conf";
    {
        std::ofstream key(selected);
        key << "key: tskey-auth-selected-value-123456\n";
    }
    auto explicitFile = loadAuthKeyFile(root.string(), selected.string());
    assert(explicitFile.found());
    assert(explicitFile.path == selected.string());

    const auto malformed = root / "bad.key";
    {
        std::ofstream key(malformed);
        key << "too-short\n";
    }
    std::filesystem::remove(root / "tailscale" / "auth-key.txt", ec);
    auto rejected = loadAuthKeyFile(root.string(), malformed.string());
    assert(!rejected.found());
    assert(!rejected.error.empty());

    std::filesystem::remove_all(root, ec);
    return 0;
}
