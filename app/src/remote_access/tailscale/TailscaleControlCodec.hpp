#pragma once

#include "TailscaleCompatibility.hpp"
#include "TailscaleTypes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace artemis::tailscale {

struct RegisterRequestData {
    std::array<std::uint8_t, 32> nodePublic{};
    std::string authKey;
    std::string hostname = "artemis-switch";
    int capabilityVersion = compat::kAcceptedCapabilityVersion;
};

struct MapRequestData {
    std::array<std::uint8_t, 32> nodePublic{};
    std::array<std::uint8_t, 32> discoPublic{};
    std::vector<std::string> endpoints;
    bool stream = true;
    // Non-streaming Hostinfo/NetInfo updates set this so control does not
    // send a peer list back.
    bool omitPeers = false;
    std::string hostname = "artemis-switch";
    // Home DERP region advertised as Hostinfo.NetInfo.PreferredDERP. Control
    // derives this node's HomeDERP from it; peers send replies there. 0 omits
    // NetInfo entirely.
    int preferredDerp = 0;
    int capabilityVersion = compat::kAcceptedCapabilityVersion;
};

struct MapUpdate {
    bool keepAlive = false;
    std::string localAddress;
    std::optional<std::vector<Peer>> fullPeers;
    PeerDelta delta;
    // Present only when the frame carried a DERPMap section. Deltas normally
    // omit it, in which case the engine keeps the last full map.
    std::optional<std::vector<DerpRegion>> derpMap;
};

// Tailscale JSON keys use a typed prefix followed by exactly 32 bytes encoded
// as 64 lowercase hexadecimal characters (for example, "nodekey:...").
// Keeping this parser shared prevents control configuration and netmap parsing
// from accepting incompatible spellings.
std::string encodeTypedKey(std::string_view prefix,
                           std::span<const std::uint8_t, 32> key);
std::optional<Key32> decodeTypedKey(std::string_view encoded,
                                    std::string_view prefix);

std::string encodeRegisterRequest(const RegisterRequestData& request);
std::string encodeMapRequest(const MapRequestData& request);

class MapCodec {
public:
    std::optional<MapUpdate> decode(std::string_view json,
                                    std::string* error = nullptr);

private:
    std::unordered_map<std::uint64_t, std::string> stableIdsByNodeId_;
};

class MapFrameDecoder {
public:
    static constexpr std::size_t kMaxFrameSize = 8 * 1024 * 1024;

    bool append(std::span<const std::uint8_t> bytes, std::string* error = nullptr);
    std::optional<std::string> take(std::string* error = nullptr);

private:
    std::vector<std::uint8_t> buffer_;
};

} // namespace artemis::tailscale
