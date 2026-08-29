#include "../app/src/remote_access/tailscale/TailscaleStateStore.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

using namespace artemis::tailscale;

int main() {
    const auto directory = std::filesystem::temp_directory_path() /
                           "artemis-tailscale-state-test";
    std::filesystem::remove_all(directory);
    const auto path = directory / "state.bin";
    StateStore store(path);
    Identity identity;
    identity.machinePrivate[0] = 1;
    identity.nodePrivate[0] = 2;
    identity.discoPrivate[0] = 3;

    std::string error;
    assert(store.save(identity, StateProtection::Plain, {}, {}, &error));
    assert(store.protection(&error) == StateProtection::Plain);
    assert(store.load({}, &error) == identity);

    SecureBytes passphrase("correct horse battery staple");
    StateKdfParameters testKdf{8 * 1024, 1, 1};
    assert(store.save(identity, StateProtection::Passphrase, passphrase.view(),
                      testKdf, &error));
    assert(store.protection(&error) == StateProtection::Passphrase);
    assert(store.load(passphrase.view(), &error) == identity);
    SecureBytes wrong("wrong");
    assert(!store.load(wrong.view(), &error));
    assert(error == "Tailscale state authentication failed");

    // A saved identity can be migrated back to a plain envelope only after
    // the protected state has been successfully opened by the caller.
    const auto unlocked = store.load(passphrase.view(), &error);
    assert(unlocked == identity);
    assert(store.save(*unlocked, StateProtection::Plain, {}, {}, &error));
    assert(store.protection(&error) == StateProtection::Plain);
    assert(store.load({}, &error) == identity);

    // Recover the last authenticated state if replacement was interrupted
    // after the primary file was moved aside.
    const auto backup = std::filesystem::path(path.string() + ".bak");
    std::filesystem::rename(path, backup);
    {
        std::ofstream temporary(path.string() + ".tmp", std::ios::binary);
        temporary << "incomplete";
    }
    assert(store.load({}, &error) == identity);
    assert(std::filesystem::exists(path));

    // Authentication catches a corrupted state before any key is returned.
    {
        std::fstream file(path, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(80);
        const char corrupt = static_cast<char>(0x5a);
        file.write(&corrupt, 1);
    }
    assert(!store.load(passphrase.view(), &error));
    assert(store.remove(&error));
    std::filesystem::remove_all(directory);
    return 0;
}
