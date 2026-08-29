#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace artemis::tailscale {

std::string buildTs2021UpgradeRequest(
    std::string_view host,
    std::span<const std::uint8_t> noiseInitiation);

// Parses the complete HTTP response header (through CRLF CRLF). No response
// body is permitted before Noise framing begins.
bool validateTs2021UpgradeResponse(std::string_view header,
                                   std::string* error = nullptr);

} // namespace artemis::tailscale
