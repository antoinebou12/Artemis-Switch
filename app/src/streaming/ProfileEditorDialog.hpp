#pragma once

#include "StreamConfigProfileStore.hpp"

#include <functional>
#include <string>

namespace artemis::streaming {

// Opens a Settings-style editor for full stream profile fields.
// If profileId is empty, creates a new profile from draft (or snapshot).
// If assignHostKey is non-empty, assigns the saved profile to that host.
void openProfileEditor(const std::string& profileId,
                       const std::string& assignHostKey,
                       const std::function<void()>& onChanged);

} // namespace artemis::streaming
