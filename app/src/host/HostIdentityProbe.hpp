#pragma once

#include "HostCapabilities.hpp"

#include <optional>
#include <string>

namespace artemis::host {

std::optional<HostIdentity> parsePunktfunkHealth(const std::string& body);
std::optional<HostIdentity> parseVibeshineWebUi(const std::string& body);

// Performs short best-effort, unauthenticated probes. Hosts that advertise the
// extended virtual-display fields are checked for Vibeshine branding first;
// otherwise (or on mismatch) Punktfunk's health endpoint is checked.
std::optional<HostIdentity> probeHostIdentity(
    const std::string& address, bool virtualDisplayHint);

std::optional<HostIdentity> probePunktfunkIdentity(
    const std::string& address);

std::string hostConsoleUrl(const std::string& address,
                           const HostIdentity& identity);

const char* punktfunkGameStreamRequiredError();

} // namespace artemis::host
