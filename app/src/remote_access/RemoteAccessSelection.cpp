#include "RemoteAccessSelection.hpp"

#include "Settings.hpp"

RemoteAccessSelectionResult
applyRemoteAccessSelection(RemoteAccessProviderId provider) {
    auto& settings = Settings::instance();

    // Clear the effective state first: if the provider fails to start, nothing
    // should claim to be connected across a restart.
    settings.set_remote_access_provider(RemoteAccessProviderId::Off);
    settings.set_wireguard_enabled(false);

    auto result = RemoteAccessManager::instance().selectAndStartProvider(
        remoteAccessProviderRuntimeId(provider));

    if (provider == RemoteAccessProviderId::Off) {
        settings.save();
        return result;
    }
    if (!result.started)
        return result;

    settings.set_remote_access_provider(provider);
    if (provider == RemoteAccessProviderId::WireGuard)
        settings.set_wireguard_enabled(true);
    settings.save();
    return result;
}
