#include "NetBirdStaticConfig.hpp"

#include <jansson.h>

#include <sstream>

namespace {

const json_t* objectValue(const json_t* object, const char* name) {
    return object && json_is_object(object) ? json_object_get(object, name)
                                            : nullptr;
}

std::string stringValue(const json_t* object, const char* name) {
    const auto* value = objectValue(object, name);
    return json_is_string(value) ? json_string_value(value) : std::string{};
}

bool appendAllowedIps(const json_t* value, std::ostringstream& output) {
    if (json_is_string(value)) {
        output << json_string_value(value);
        return true;
    }
    if (!json_is_array(value) || json_array_size(value) == 0)
        return false;

    for (size_t index = 0; index < json_array_size(value); ++index) {
        const auto* item = json_array_get(value, index);
        if (!json_is_string(item))
            return false;
        if (index != 0)
            output << ", ";
        output << json_string_value(item);
    }
    return true;
}

} // namespace

NetBirdStaticConfigResult
parse_netbird_static_config(std::string_view jsonText) {
    NetBirdStaticConfigResult result;
    json_error_t error{};
    json_auto_t* root = json_loadb(jsonText.data(), jsonText.size(), 0, &error);
    if (!root || !json_is_object(root)) {
        result.error = "invalid_json";
        return result;
    }

    const auto privateKey = stringValue(root, "PrivateKey");
    const auto address = stringValue(root, "Address");
    const auto* peers = objectValue(root, "Peers");
    if (privateKey.empty() || address.empty() || !json_is_array(peers) ||
        json_array_size(peers) == 0) {
        result.error = "missing_static_fields";
        return result;
    }

    std::ostringstream output;
    output << "[Interface]\n"
           << "PrivateKey = " << privateKey << "\n"
           << "Address = " << address << "\n";

    const auto listenPort = objectValue(root, "ListenPort");
    if (json_is_integer(listenPort))
        output << "ListenPort = " << json_integer_value(listenPort) << "\n";
    const auto mtu = objectValue(root, "MTU");
    if (json_is_integer(mtu))
        output << "MTU = " << json_integer_value(mtu) << "\n";

    for (size_t index = 0; index < json_array_size(peers); ++index) {
        const auto* peer = json_array_get(peers, index);
        if (!json_is_object(peer)) {
            result.error = "invalid_peer";
            return result;
        }

        const auto publicKey = stringValue(peer, "PublicKey");
        const auto endpoint = stringValue(peer, "Endpoint");
        const auto* allowedIps = objectValue(peer, "AllowedIPs");
        if (publicKey.empty() || endpoint.empty() || !allowedIps) {
            result.error = "missing_peer_fields";
            return result;
        }

        std::ostringstream allowed;
        if (!appendAllowedIps(allowedIps, allowed)) {
            result.error = "invalid_allowed_ips";
            return result;
        }

        output << "\n[Peer]\n"
               << "PublicKey = " << publicKey << "\n"
               << "Endpoint = " << endpoint << "\n"
               << "AllowedIPs = " << allowed.str() << "\n";

        const auto presharedKey = stringValue(peer, "PresharedKey");
        if (!presharedKey.empty())
            output << "PresharedKey = " << presharedKey << "\n";
        const auto keepalive = objectValue(peer, "PersistentKeepalive");
        if (json_is_integer(keepalive))
            output << "PersistentKeepalive = "
                   << json_integer_value(keepalive) << "\n";
    }

    result.wireguardConfig = output.str();
    return result;
}
