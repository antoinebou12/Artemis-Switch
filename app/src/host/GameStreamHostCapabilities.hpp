#pragma once

#include "HostCapabilities.hpp"
#include "client.h"

namespace artemis::host {

HostMetadata metadataFromServerData(const SERVER_DATA& server);
HostCapabilities detectServerCapabilities(const SERVER_DATA& server);

} // namespace artemis::host
