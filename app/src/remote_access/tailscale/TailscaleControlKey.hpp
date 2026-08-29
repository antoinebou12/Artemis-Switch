#pragma once

#include "TailscaleTransport.hpp"
#include "TailscaleTypes.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace artemis::tailscale {

// The public control key is fetched outside TS2021 over a separately verified
// TLS connection. Parsing is kept independent from the platform TLS adapter so
// malformed and oversized responses can be tested deterministically.
std::optional<Key32> parseControlPublicKeyResponse(
    std::string_view json, std::string* error = nullptr);

std::optional<Key32> fetchControlPublicKey(
    ITransport& verifiedTls, std::string_view host, std::uint16_t port,
    int capabilityVersion, std::string* error = nullptr);

} // namespace artemis::tailscale
