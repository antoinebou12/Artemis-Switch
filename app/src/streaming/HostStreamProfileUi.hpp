#pragma once

#include "StreamConfigProfileStore.hpp"

#include <functional>
#include <string>

namespace artemis::streaming {

std::string profile_detail_label(const std::string& hostKey);

void open_host_profile_picker(const std::string& hostKey,
                              const std::function<void()>& onChanged);

void open_create_host_profile(const std::string& hostKey,
                              const std::function<void()>& onChanged);

void open_edit_profile(const std::string& profileId,
                       const std::function<void()>& onChanged);

void open_manage_host_profile(const std::string& hostKey,
                              const std::function<void()>& onChanged);

} // namespace artemis::streaming
