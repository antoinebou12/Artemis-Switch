#include "RemoteAccessSelection.hpp"

#include "Settings.hpp"
#include "helper.hpp"

#include <borealis.hpp>

using namespace brls::literals;

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

void applyRemoteAccessSelectionAsync(
    RemoteAccessProviderId provider,
    std::shared_ptr<std::atomic<bool>> alive,
    std::function<void(const RemoteAccessSelectionResult&)> onDone) {
    // netbird_init() is a full network login, and netbird_shutdown() joins the
    // relay, WireGuard, keepalive and proxy threads. Either one on the UI thread
    // freezes the app for as long as it takes.
    const bool connecting = provider != RemoteAccessProviderId::Off;
    brls::Dialog* loading = createLoadingDialog(
        connecting ? "settings/remote_access_connecting"_i18n
                   : "settings/remote_access_disconnecting"_i18n);
    loading->setCancelable(false);
    loading->open();

    brls::async([provider, alive, onDone, loading]() {
        const auto result = applyRemoteAccessSelection(provider);
        brls::sync([alive, onDone, loading, result]() {
            loading->close();
            // The view that asked for this may be gone by now.
            if (alive && !alive->load()) {
                return;
            }
            if (onDone) {
                onDone(result);
            }
        });
    });
}
