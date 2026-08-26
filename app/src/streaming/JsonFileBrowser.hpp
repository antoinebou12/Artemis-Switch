#pragma once

#include <functional>
#include <string>
#include <vector>

namespace artemis::streaming {

enum class JsonFileBrowserMode { Import, Export };

// In-app folder browser under working dir / sdmc:/.
// Import: pick a file matching `extensions` (default: *.json).
// Export: pick a folder, then IME for filename.
//
// `extensions` entries may be given with or without a leading dot and are
// matched case-insensitively. An empty list matches every file.
void openFileBrowser(JsonFileBrowserMode mode,
                     const std::vector<std::string>& extensions,
                     const std::string& title,
                     const std::function<void(const std::string& path)>&
                         onPicked);

// Backwards-compatible *.json entry point used by the profile import/export UI.
void openJsonFileBrowser(JsonFileBrowserMode mode,
                         const std::function<void(const std::string& path)>&
                             onPicked);

} // namespace artemis::streaming
