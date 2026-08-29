#include "../app/src/remote_access/tailscale/TailscaleControlCodec.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

using namespace artemis::tailscale;

int main() {
    RegisterRequestData registration;
    registration.nodePublic.fill(7);
    registration.authKey = "tskey-auth-test";
    assert(encodeRegisterRequest(registration).empty());
    registration.capabilityVersion = 68; // dedicated-probe candidate
    const auto registerJson = encodeRegisterRequest(registration);
    assert(registerJson.find("\"Version\":68") != std::string::npos);
    assert(registerJson.find("tskey-auth-test") != std::string::npos);
    assert(registerJson.find(
               "nodekey:0707070707070707070707070707070707070707070707070707070707070707") !=
           std::string::npos);

    MapRequestData request;
    request.nodePublic.fill(1);
    request.discoPublic.fill(2);
    assert(encodeMapRequest(request).empty());
    request.capabilityVersion = 68; // dedicated-probe candidate
    const auto requestJson = encodeMapRequest(request);
    assert(requestJson.find("nodekey:") != std::string::npos);
    assert(requestJson.find("discokey:") != std::string::npos);

    // Official key text is a typed prefix plus 64 lowercase hex digits.
    const std::string zeroKey =
        "0000000000000000000000000000000000000000000000000000000000000000";
    const std::string full =
        "{\"Node\":{\"Addresses\":[\"100.64.0.2/32\"]},"
        "\"UnknownFutureField\":true,\"Peers\":[{"
        "\"ID\":42,\"StableID\":\"stable-42\","
        "\"Name\":\"gaming-pc\",\"Key\":\"nodekey:" + zeroKey +
        "\",\"DiscoKey\":\"discokey:" + zeroKey +
        "\",\"Addresses\":[\"100.64.0.42/32\",\"fd7a::42/128\"],"
        "\"Endpoints\":[\"192.0.2.42:41641\"],\"HomeDERP\":10,"
        "\"Online\":true}]}";
    MapCodec codec;
    std::string error;
    const auto update = codec.decode(full, &error);
    assert(update && update->localAddress == "100.64.0.2");
    assert(update->fullPeers && update->fullPeers->size() == 1);
    assert(update->fullPeers->front().stableId == "stable-42");
    assert(update->fullPeers->front().endpoints.front().port == 41641);

    const auto removed = codec.decode("{\"PeersRemoved\":[42]}", &error);
    assert(removed && removed->delta.removedStableIds.size() == 1);
    assert(removed->delta.removedStableIds.front() == "stable-42");

    const std::string missingStable =
        "{\"PeersChanged\":[{\"ID\":43,\"Key\":\"nodekey:" +
        zeroKey + "\"}]}";
    assert(!codec.decode(missingStable, &error));
    assert(error == "netmap peer has no stable identity");

    // A malformed delta is transactional: it cannot install the numeric ID
    // mapping used by a later removal.
    const auto unknownRemoval = codec.decode("{\"PeersRemoved\":[43]}", &error);
    assert(unknownRemoval && unknownRemoval->delta.removedStableIds.empty());

    const std::string nonCanonicalKey =
        "{\"PeersChanged\":[{\"ID\":44,\"StableID\":\"stable-44\","
        "\"Key\":\"nodekey:" + zeroKey.substr(0, 42) + "\"}]}";
    assert(!codec.decode(nonCanonicalKey, &error));
    assert(error == "netmap peer has an invalid node key");

    const std::string uppercaseKey =
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    assert(!decodeTypedKey("nodekey:" + uppercaseKey, "nodekey:"));
    const auto decodedZero = decodeTypedKey("nodekey:" + zeroKey, "nodekey:");
    assert(decodedZero && decodedZero->at(0) == 0 && decodedZero->at(31) == 0);
    assert(encodeTypedKey("nodekey:", *decodedZero) == "nodekey:" + zeroKey);

    const std::string keepAlive = "{\"KeepAlive\":true}";
    std::vector<std::uint8_t> framed(4 + keepAlive.size());
    const auto size = static_cast<std::uint32_t>(keepAlive.size());
    framed[0] = static_cast<std::uint8_t>(size);
    framed[1] = static_cast<std::uint8_t>(size >> 8U);
    framed[2] = static_cast<std::uint8_t>(size >> 16U);
    framed[3] = static_cast<std::uint8_t>(size >> 24U);
    std::copy(keepAlive.begin(), keepAlive.end(), framed.begin() + 4);
    MapFrameDecoder decoder;
    assert(decoder.append(std::span(framed).first(5), &error));
    assert(!decoder.take(&error));
    assert(decoder.append(std::span(framed).subspan(5), &error));
    assert(decoder.take(&error) == keepAlive);
    return 0;
}
