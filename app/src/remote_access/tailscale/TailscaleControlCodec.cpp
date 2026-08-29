#include "TailscaleControlCodec.hpp"

#include "TailscaleCompatibility.hpp"
#include "TailscalePeerDirectory.hpp"

#include <borealis/extern/nlohmann/json.hpp>

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
    if (peer.addresses.empty()) {
        if (const auto allowed = node.find("AllowedIPs");
            allowed != node.end() && allowed->is_array()) {
            if (allowed->size() > 32) {
                if (error) *error = "netmap peer address limit exceeded";
                return std::nullopt;
            }
            for (const auto& value : *allowed) {
                if (value.is_string())
                    peer.addresses.push_back(stripPrefix(value.get<std::string>()));
            }
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
    Json root = {
        {"Version", request.capabilityVersion},
        {"NodeKey", encodeTypedKey("nodekey:", request.nodePublic)},
        {"DiscoKey", encodeTypedKey("discokey:", request.discoPublic)},
        {"Stream", request.stream},
        {"ReadOnly", false},
        {"OmitPeers", false},
        {"Compress", ""},
        {"Endpoints", request.endpoints},
        {"Hostinfo", {
            {"Hostname", "artemis-switch"},
            {"OS", "nintendo-switch"},
        }},
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
