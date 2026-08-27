#include <cassert>

#include "../app/src/streaming/FileBrowserPath.hpp"

int main() {
    using artemis::streaming::ensureFileExtension;
    using artemis::streaming::fileMatchesExtensions;

    assert(fileMatchesExtensions("profile.JSON", {".json"}));
    assert(!fileMatchesExtensions("profile.conf", {"json"}));
    assert(fileMatchesExtensions("wg0.conf", {}));
    assert(fileMatchesExtensions("tailscaled.state", {}));
    assert(ensureFileExtension("profile", {"json"}) == "profile.json");
    assert(ensureFileExtension("profile.JSON", {".json"}) == "profile.JSON");
    return 0;
}
