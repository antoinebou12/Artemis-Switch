#pragma once

#include "RemoteAccessManager.hpp"
#include "remote_access_provider_id.hpp"

// Applies the selected provider as desired state. Effective enabled flags are
// cleared first and only set again when the provider really starts.
RemoteAccessSelectionResult
applyRemoteAccessSelection(RemoteAccessProviderId provider);
