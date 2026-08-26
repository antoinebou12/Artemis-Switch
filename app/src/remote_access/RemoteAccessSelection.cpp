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

void disconnectRemoteAccess() {
    // Deliberately does not touch remote_access_provider: the user picked that
    // provider, and disconnecting is a transport action, not a change of mind.
    RemoteAccessManager::instance().stopActiveProvider();
    Settings::instance().set_wireguard_enabled(false);
    Settings::instance().save();
}

namespace {

// Runs `work` on a worker thread behind a modal dialog, then `finish` on the UI
// thread once the dialog has finished closing.
//
// The dialog is closed via close(cb) rather than close() followed by more
// statements: the follow-up has to run after the close animation completes, or
// it races the activity stack.
void runBehindLoadingDialog(const std::string& message,
                            std::shared_ptr<std::atomic<bool>> alive,
                            std::function<void()> work,
                            std::function<void()> finish) {
    brls::Dialog* loading = createLoadingDialog(message);
    loading->setCancelable(false);
    loading->open();

    brls::async([alive, work, finish, loading]() {
        if (work) {
            work();
        }
        brls::sync([alive, finish, loading]() {
            loading->close([alive, finish]() {
                // The view that asked for this may be gone by now.
                if (alive && !alive->load()) {
                    return;
                }
                if (finish) {
                    finish();
                }
            });
        });
    });
}

} // namespace

void applyRemoteAccessSelectionAsync(
    RemoteAccessProviderId provider,
    std::shared_ptr<std::atomic<bool>> alive,
    std::function<void(const RemoteAccessSelectionResult&)> onDone) {
    // netbird_init() is a full network login, and netbird_shutdown() joins the
    // relay, WireGuard, keepalive and proxy threads. Either one on the UI thread
    // freezes the app for as long as it takes.
    auto result = std::make_shared<RemoteAccessSelectionResult>();
    const bool connecting = provider != RemoteAccessProviderId::Off;

    runBehindLoadingDialog(
        connecting ? "settings/remote_access_connecting"_i18n
                   : "settings/remote_access_disconnecting"_i18n,
        alive,
        [provider, result]() { *result = applyRemoteAccessSelection(provider); },
        [onDone, result]() {
            if (onDone) {
                onDone(*result);
            }
        });
}

void disconnectRemoteAccessAsync(std::shared_ptr<std::atomic<bool>> alive,
                                 std::function<void()> onDone) {
    runBehindLoadingDialog("settings/remote_access_disconnecting"_i18n, alive,
                           []() { disconnectRemoteAccess(); },
                           std::move(onDone));
}
