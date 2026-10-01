#include "TailscaleControlCodec.hpp"

#include "TailscaleCompatibility.hpp"
#include "TailscalePeerDirectory.hpp"

#include <borealis/extern/nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <limits>

namespace artemis::tailscale {
namespace {

using Json = nlohmann::json;

std::string stripPrefix(std::string address) {
    const auto slash = address.find('/');
    if (slash != std::string::npos)
        address.resize(slash);
    return address;
}

// Hostinfo.OS and the TCP ports in Hostinfo.Services, used to tell game
// hosts from phones and servers. Every field is optional and type-checked:
// control may omit, null or trim any of it.
void parseHostinfo(const Json& node, Peer& peer) {
    const auto hostinfo = node.find("Hostinfo");
    if (hostinfo == node.end() || !hostinfo->is_object())
        return;
    if (const auto os = hostinfo->find("OS");
        os != hostinfo->end() && os->is_string()) {
        peer.os = os->get<std::string>();
        if (peer.os.size() > 32)
            peer.os.resize(32);
        std::transform(peer.os.begin(), peer.os.end(), peer.os.begin(),
                       [](unsigned char c) {
                           return static_cast<char>(std::tolower(c));
                       });
    }
    const auto services = hostinfo->find("Services");
    if (services == hostinfo->end() || !services->is_array())
        return;
    peer.servicesKnown = true;
    constexpr std::size_t kMaxServices = 256;
    for (const auto& service : *services) {
        if (peer.tcpServicePorts.size() >= kMaxServices)
            break;
        if (!service.is_object())
            continue;
        const auto proto = service.find("Proto");
        const auto port = service.find("Port");
        if (proto == service.end() || !proto->is_string() ||
            proto->get<std::string>() != "tcp" || port == service.end() ||
            !port->is_number_unsigned())
            continue;
        const auto value = port->get<std::uint64_t>();
        if (value == 0 || value > 65535)
            continue;
        const auto tcpPort = static_cast<std::uint16_t>(value);
        if (std::find(peer.tcpServicePorts.begin(), peer.tcpServicePorts.end(),
                      tcpPort) == peer.tcpServicePorts.end())
            peer.tcpServicePorts.push_back(tcpPort);
    }
}

std::optional<Peer> parsePeer(
    const Json& node, std::unordered_map<std::uint64_t, std::string>& idMap,
    std::string* error) {
    if (!node.is_object()) {
        if (error) *error = "netmap peer is not an object";
        return std::nullopt;
    }
    Peer peer;
    peer.stableId = node.value("StableID", std::string{});
    const auto nodeId = node.value("ID", std::uint64_t{});
    if (peer.stableId.empty()) {
        if (error) *error = "netmap peer has no stable identity";
        return std::nullopt;
    }
    peer.hostname = node.value("Name", std::string{});
    if (const auto key = decodeTypedKey(node.value("Key", std::string{}),
                                        "nodekey:"))
        peer.nodeKey = *key;
    else {
        if (error) *error = "netmap peer has an invalid node key";
        return std::nullopt;
    }
    const auto discoText = node.value("DiscoKey", std::string{});
    if (!discoText.empty()) {
        if (const auto key = decodeTypedKey(discoText, "discokey:"))
            peer.discoKey = *key;
        else {
            if (error) *error = "netmap peer has an invalid disco key";
            return std::nullopt;
        }
    }
    if (const auto addresses = node.find("Addresses");
        addresses != node.end() && addresses->is_array()) {
        if (addresses->size() > 32) {
            if (error) *error = "netmap peer address limit exceeded";
            return std::nullopt;
        }
        for (const auto& value : *addresses) {
            if (value.is_string())
                peer.addresses.push_back(stripPrefix(value.get<std::string>()));
        }
    }
    if (const auto allowed = node.find("AllowedIPs");
        allowed != node.end() && allowed->is_array()) {
        const bool addressesFromAllowed = peer.addresses.empty();
        if (addressesFromAllowed && allowed->size() > 32) {
            if (error) *error = "netmap peer address limit exceeded";
            return std::nullopt;
        }
        for (const auto& value : *allowed) {
            if (!value.is_string())
                continue;
            // A subnet router can advertise many routes; keep a bounded set
            // instead of rejecting the whole peer.
            if (!addressesFromAllowed &&
                peer.allowedIPs.size() >= PeerDirectory::kMaxAllowedIPsPerPeer)
                break;
            const auto cidr = value.get<std::string>();
            if (addressesFromAllowed) {
                peer.addresses.push_back(stripPrefix(cidr));
                continue;
            }
            // Subnet routes advertised by this peer (e.g. an OpenWrt subnet
            // router sharing 192.168.1.0/24). Exit-node default routes and
            // the peer's own /32 are not LAN subnets and are skipped.
            if (PeerDirectory::isRoutableIPv4Subnet(cidr, peer.addresses))
                peer.allowedIPs.push_back(cidr);
        }
    }
    if (const auto endpoints = node.find("Endpoints");
        endpoints != node.end() && endpoints->is_array()) {
        if (endpoints->size() > PeerDirectory::kMaxEndpointsPerPeer) {
            if (error) *error = "netmap peer endpoint limit exceeded";
            return std::nullopt;
        }
        for (const auto& value : *endpoints) {
            if (!value.is_string())
                continue;
            const auto endpointText = value.get<std::string>();
            const auto colon = endpointText.rfind(':');
            if (colon == std::string::npos)
                continue;
            unsigned int port = 0;
            const auto portText = std::string_view(endpointText).substr(colon + 1);
            const auto result = std::from_chars(portText.data(),
                                                portText.data() + portText.size(),
                                                port);
            const auto host = endpointText.substr(0, colon);
            if (result.ec != std::errc{} ||
                result.ptr != portText.data() + portText.size() || port == 0 ||
                port > 65535 || !PeerDirectory::isLiteralIPv4(host))
                continue;
            peer.endpoints.push_back(
                {host, static_cast<std::uint16_t>(port)});
        }
    }
    peer.homeDerp = node.value("HomeDERP", 0);
    if (peer.homeDerp == 0) {
        const auto legacy = node.value("DERP", std::string{});
        const auto colon = legacy.rfind(':');
        if (colon != std::string::npos) {
            const auto region = std::string_view(legacy).substr(colon + 1);
            std::from_chars(region.data(), region.data() + region.size(),
                            peer.homeDerp);
        }
    }
    peer.online = node.value("Online", false);
    parseHostinfo(node, peer);
    if (nodeId != 0)
        idMap[nodeId] = peer.stableId;
    return peer;
}

std::string localAddressFromNode(const Json& root) {
    const auto node = root.find("Node");
    if (node == root.end() || !node->is_object())
        return {};
    const auto addresses = node->find("Addresses");
    if (addresses == node->end() || !addresses->is_array())
        return {};
    for (const auto& value : *addresses) {
        if (!value.is_string())
            continue;
        auto candidate = stripPrefix(value.get<std::string>());
        if (PeerDirectory::isLiteralIPv4(candidate))
            return candidate;
    }
    return {};
}

constexpr std::size_t kMaxDerpRegions = 64;
constexpr std::size_t kMaxDerpNodesPerRegion = 16;

bool isPlausibleHostname(std::string_view host) {
    if (host.empty() || host.size() > 253)
        return false;
    for (const char c : host) {
        if (c <= ' ' || c == 127 || c == '/' || c == ':' || c == '@')
            return false;
    }
    return true;
}

// Parses the control plane's DERPMap section:
// {"Regions": {"1": {"RegionID": 1, "RegionCode": "nyc",
//   "Nodes": [{"HostName": "derp1.example", "DERPPort": 443, "IPv4": "203.0.113.7"}]}}}
// Returns nullopt with *error set when the section is present but malformed;
// callers treat a missing section as "no update" rather than a failure.
std::optional<std::vector<DerpRegion>> parseDerpMap(const Json& root,
                                                    std::string* error) {
    const auto derpMap = root.find("DERPMap");
    if (derpMap == root.end() || derpMap->is_null())
        return std::nullopt;
    if (!derpMap->is_object()) {
        if (error) *error = "netmap DERPMap is not an object";
        return std::nullopt;
    }
    const auto regions = derpMap->find("Regions");
    if (regions == derpMap->end() || regions->is_null())
        return std::nullopt;
    if (!regions->is_object()) {
        if (error) *error = "netmap DERPMap.Regions is not an object";
        return std::nullopt;
    }
    if (regions->size() > kMaxDerpRegions) {
        if (error) *error = "netmap DERP region limit exceeded";
        return std::nullopt;
    }
    std::vector<DerpRegion> out;
    out.reserve(regions->size());
    for (const auto& [key, region] : regions->items()) {
        if (!region.is_object())
            continue;
        DerpRegion entry;
        entry.regionId = region.value("RegionID", 0);
        if (entry.regionId == 0) {
            unsigned int parsed = 0;
            const auto result = std::from_chars(key.data(), key.data() + key.size(), parsed);
            if (result.ec == std::errc{} && result.ptr == key.data() + key.size() && parsed > 0 &&
                parsed <= static_cast<unsigned int>(std::numeric_limits<int>::max()))
                entry.regionId = static_cast<int>(parsed);
        }
        if (entry.regionId <= 0)
            continue;
        entry.regionCode = region.value("RegionCode", std::string{});
        const auto nodes = region.find("Nodes");
        if (nodes != region.end() && nodes->is_array()) {
            for (const auto& node : *nodes) {
                if (entry.nodes.size() >= kMaxDerpNodesPerRegion)
                    break;
                if (!node.is_object())
                    continue;
                DerpNode derpNode;
                auto host = node.value("HostName", std::string{});
                if (host.empty())
                    host = node.value("IPv4", std::string{});
                if (!isPlausibleHostname(host))
                    continue;
                derpNode.host = std::move(host);
                const auto port = node.value("DERPPort", 443);
                if (port <= 0 || port > 65535)
                    continue;
                derpNode.port = static_cast<std::uint16_t>(port);
                // STUNPort: 0 means the default 3478, negative disables it.
                const int stunPort = node.value("STUNPort", 0);
                derpNode.stunPort =
                    stunPort < 0 || stunPort > 65535
                        ? std::uint16_t{0}
                        : static_cast<std::uint16_t>(stunPort == 0 ? 3478
                                                                   : stunPort);
                if (node.value("STUNOnly", false))
                    continue; // no DERP service on this node
                entry.nodes.push_back(std::move(derpNode));
            }
        }
        if (entry.nodes.empty())
            continue;
        out.push_back(std::move(entry));
    }
    return out;
}

} // namespace

std::string encodeTypedKey(std::string_view prefix,
                           std::span<const std::uint8_t, 32> key) {
    constexpr char kHex[] = "0123456789abcdef";
    std::string output(prefix);
    output.reserve(prefix.size() + key.size() * 2);
    for (const auto byte : key) {
        output.push_back(kHex[byte >> 4U]);
        output.push_back(kHex[byte & 0x0fU]);
    }
    return output;
}

std::optional<Key32> decodeTypedKey(std::string_view encoded,
                                    std::string_view prefix) {
    if (!encoded.starts_with(prefix))
        return std::nullopt;
    encoded.remove_prefix(prefix.size());
    if (encoded.size() != Key32{}.size() * 2)
        return std::nullopt;

    auto nibble = [](const char value) -> std::optional<std::uint8_t> {
        if (value >= '0' && value <= '9')
            return static_cast<std::uint8_t>(value - '0');
        if (value >= 'a' && value <= 'f')
            return static_cast<std::uint8_t>(value - 'a' + 10);
        return std::nullopt;
    };

    Key32 result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        const auto high = nibble(encoded[index * 2]);
        const auto low = nibble(encoded[index * 2 + 1]);
        if (!high || !low)
            return std::nullopt;
        result[index] = static_cast<std::uint8_t>((*high << 4U) | *low);
    }
    return result;
}

std::string encodeRegisterRequest(const RegisterRequestData& request) {
    if (request.capabilityVersion <= 0)
        return {};
    Json root = {
        {"Version", request.capabilityVersion},
        {"NodeKey", encodeTypedKey("nodekey:", request.nodePublic)},
        {"Auth", { {"AuthKey", request.authKey} }},
        {"Hostinfo", {
            {"Hostname", request.hostname},
            {"OS", "nintendo-switch"},
            {"Package", "artemis-switch"},
        }},
        {"Followup", ""},
    };
    return root.dump();
}

std::string encodeMapRequest(const MapRequestData& request) {
    if (request.capabilityVersion <= 0)
        return {};
    Json hostinfo = {
        {"Hostname", request.hostname.empty() ? std::string("artemis-switch")
                                              : request.hostname},
        {"OS", "nintendo-switch"},
    };
    if (request.preferredDerp > 0)
        hostinfo["NetInfo"] = {{"PreferredDERP", request.preferredDerp}};
    Json root = {
        {"Version", request.capabilityVersion},
        {"NodeKey", encodeTypedKey("nodekey:", request.nodePublic)},
        {"DiscoKey", encodeTypedKey("discokey:", request.discoPublic)},
        {"Stream", request.stream},
        {"ReadOnly", false},
        {"OmitPeers", request.omitPeers},
        {"Compress", ""},
        {"Endpoints", request.endpoints},
        {"Hostinfo", std::move(hostinfo)},
    };
    return root.dump();
}

std::optional<MapUpdate> MapCodec::decode(std::string_view json,
                                          std::string* error) {
    if (json.size() > MapFrameDecoder::kMaxFrameSize) {
        if (error) *error = "netmap JSON is oversized";
        return std::nullopt;
    }
    const auto root = Json::parse(json, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        if (error) *error = "invalid netmap JSON";
        return std::nullopt;
    }
    MapUpdate update;
    update.keepAlive = root.value("KeepAlive", false);
    if (update.keepAlive)
        return update;
    update.localAddress = localAddressFromNode(root);

    // A malformed DERPMap section fails the update: relay selection without
    // trustworthy region data would blackhole the data path. Absent sections
    // (typical for deltas) leave the engine's stored map untouched.
    if (const auto derpIt = root.find("DERPMap");
        derpIt != root.end() && !derpIt->is_null()) {
        auto regions = parseDerpMap(root, error);
        if (!regions)
            return std::nullopt;
        update.derpMap = std::move(*regions);
    }

    // Decode into a copy and commit the ID mapping only if the entire update
    // validates. A malformed later peer must not poison subsequent deltas.
    auto nextIdMap = stableIdsByNodeId_;

    if (const auto peers = root.find("Peers");
        peers != root.end() && peers->is_array()) {
        if (peers->size() > PeerDirectory::kMaxPeers) {
            if (error) *error = "netmap peer limit exceeded";
            return std::nullopt;
        }
        nextIdMap.clear();
        std::vector<Peer> full;
        full.reserve(peers->size());
        for (const auto& node : *peers) {
            auto peer = parsePeer(node, nextIdMap, error);
            if (!peer)
                return std::nullopt;
            full.push_back(std::move(*peer));
        }
        update.fullPeers = std::move(full);
    }
    if (const auto changed = root.find("PeersChanged");
        changed != root.end() && changed->is_array()) {
        if (changed->size() > PeerDirectory::kMaxPeers) {
            if (error) *error = "netmap peer delta limit exceeded";
            return std::nullopt;
        }
        for (const auto& node : *changed) {
            auto peer = parsePeer(node, nextIdMap, error);
            if (!peer)
                return std::nullopt;
            update.delta.changed.push_back(std::move(*peer));
        }
    }
    if (const auto removed = root.find("PeersRemoved");
        removed != root.end() && removed->is_array()) {
        if (removed->size() > PeerDirectory::kMaxPeers) {
            if (error) *error = "netmap peer removal limit exceeded";
            return std::nullopt;
        }
        for (const auto& value : *removed) {
            if (!value.is_number_unsigned())
                continue;
            const auto nodeId = value.get<std::uint64_t>();
            const auto found = nextIdMap.find(nodeId);
            if (found != nextIdMap.end()) {
                update.delta.removedStableIds.push_back(found->second);
                nextIdMap.erase(found);
            }
        }
    }
    // Official control planes also emit PeersChangedPatch frames carrying
    // partial per-node updates (presence, LastSeen, ...). Only the Online
    // flag is actionable here; anything else is forward-compatible noise.
    // Unknown or missing NodeIDs are skipped: the peer is either gone or from
    // a future schema, and neither case may fail the update.
    if (const auto patches = root.find("PeersChangedPatch");
        patches != root.end() && patches->is_array()) {
        if (patches->size() > PeerDirectory::kMaxPeers) {
            if (error) *error = "netmap peer patch limit exceeded";
            return std::nullopt;
        }
        for (const auto& patch : *patches) {
            if (!patch.is_object())
                continue;
            const auto nodeIdIt = patch.find("NodeID");
            const auto onlineIt = patch.find("Online");
            if (nodeIdIt == patch.end() || !nodeIdIt->is_number_unsigned() ||
                onlineIt == patch.end() || !onlineIt->is_boolean())
                continue;
            const auto found = nextIdMap.find(nodeIdIt->get<std::uint64_t>());
            if (found == nextIdMap.end())
                continue;
            PeerOnlineChange change;
            change.stableId = found->second;
            change.online = onlineIt->get<bool>();
            update.delta.onlineChanges.push_back(std::move(change));
        }
    }
    stableIdsByNodeId_ = std::move(nextIdMap);
    return update;
}

bool MapFrameDecoder::append(std::span<const std::uint8_t> bytes,
                             std::string* error) {
    if (bytes.size() > kMaxFrameSize + 4 ||
        buffer_.size() > kMaxFrameSize + 4 - bytes.size()) {
        if (error) *error = "netmap receive buffer limit exceeded";
        buffer_.clear();
        return false;
    }
    buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
    return true;
}

std::optional<std::string> MapFrameDecoder::take(std::string* error) {
    if (buffer_.size() < 4)
        return std::nullopt;
    const std::uint32_t size = static_cast<std::uint32_t>(buffer_[0]) |
                               static_cast<std::uint32_t>(buffer_[1]) << 8U |
                               static_cast<std::uint32_t>(buffer_[2]) << 16U |
                               static_cast<std::uint32_t>(buffer_[3]) << 24U;
    if (size > kMaxFrameSize) {
        if (error) *error = "netmap frame is oversized";
        buffer_.clear();
        return std::nullopt;
    }
    if (buffer_.size() < static_cast<std::size_t>(size) + 4)
        return std::nullopt;
    std::string result(buffer_.begin() + 4,
                       buffer_.begin() + 4 + static_cast<std::ptrdiff_t>(size));
    buffer_.erase(buffer_.begin(),
                  buffer_.begin() + 4 + static_cast<std::ptrdiff_t>(size));
    return result;
}

} // namespace artemis::tailscale
