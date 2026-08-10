#pragma once

#include <functional>
#include <string>

namespace artemis::streaming {

enum class JsonFileBrowserMode { Import, Export };

// In-app folder browser under working dir / sdmc:/.
// Import: pick a *.json file.
// Export: pick a folder, then IME for filename (default profile_export.json).
void openJsonFileBrowser(JsonFileBrowserMode mode,
                         const std::function<void(const std::string& path)>&
                             onPicked);

} // namespace artemis::streaming
