#pragma once

#include "RemoteAccessManager.hpp"
#include "remote_access_provider_id.hpp"

#include <functional>
#include <memory>

// BLOCKING. Applies the selected provider as desired state; effective flags are
// cleared first and only set again when the provider really starts.
//
// This performs a network login and, on teardown, joins the VPN worker threads.
// Do NOT call it from the UI thread -- use applyRemoteAccessSelectionAsync().
RemoteAccessSelectionResult
applyRemoteAccessSelection(RemoteAccessProviderId provider);

// UI-safe wrapper: runs the blocking work on a worker thread behind a modal
// loading dialog and delivers the result back on the UI thread.
//
// `alive` is checked after the worker finishes; pass a flag owned by the calling
// view and cleared in its destructor so a view torn down mid-connect does not
// get a callback into freed memory.
void applyRemoteAccessSelectionAsync(
    RemoteAccessProviderId provider,
    std::shared_ptr<std::atomic<bool>> alive,
    std::function<void(const RemoteAccessSelectionResult&)> onDone);
