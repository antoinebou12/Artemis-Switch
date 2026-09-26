#include "../app/src/remote_access/tailscale/TailscaleControlCodec.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

using namespace artemis::tailscale;

int main() {
    RegisterRequestData registration;
    registration.capabilityVersion = 0;
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
    request.capabilityVersion = 0;
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

    // Frames without a DERPMap section leave the update empty-handed; the
    // engine keeps its stored relay map.
    assert(!update->derpMap.has_value());

    // A DERPMap section decodes to relay regions. HostName wins over IPv4,
    // DERPPort defaults to 443, and unusable regions are skipped.
    const std::string withDerp =
        "{\"Node\":{\"Addresses\":[\"100.64.0.2/32\"]},\"Peers\":[],"
        "\"DERPMap\":{\"Regions\":{"
        "\"1\":{\"RegionID\":1,\"RegionCode\":\"nyc\",\"Nodes\":["
        "{\"Name\":\"1a\",\"RegionID\":1,\"HostName\":\"derp1.example\","
        "\"IPv4\":\"203.0.2.1\",\"DERPPort\":443},"
        "{\"Name\":\"1b\",\"RegionID\":1,\"IPv4\":\"203.0.2.2\"}]},"
        "\"2\":{\"RegionID\":2,\"RegionCode\":\"fra\",\"Nodes\":["
        "{\"Name\":\"2a\",\"RegionID\":2,\"HostName\":\"derp2.example\","
        "\"DERPPort\":8443}]},"
        "\"3\":{\"RegionID\":3,\"RegionCode\":\"empty\",\"Nodes\":["
        "{\"Name\":\"3a\",\"RegionID\":3}]}}}}";
    const auto derpUpdate = codec.decode(withDerp, &error);
    assert(derpUpdate && derpUpdate->derpMap.has_value());
    assert(derpUpdate->derpMap->size() == 2);
    const DerpRegion* nyc = nullptr;
    const DerpRegion* fra = nullptr;
    for (const auto& region : *derpUpdate->derpMap) {
        if (region.regionId == 1) nyc = &region;
        if (region.regionId == 2) fra = &region;
    }
    assert(nyc && nyc->regionCode == "nyc" && nyc->nodes.size() == 2);
    assert(nyc->nodes[0].host == "derp1.example" && nyc->nodes[0].port == 443);
    assert(nyc->nodes[1].host == "203.0.2.2" && nyc->nodes[1].port == 443);
    assert(fra && fra->nodes.size() == 1);
    assert(fra->nodes[0].host == "derp2.example" && fra->nodes[0].port == 8443);

    // A malformed DERPMap section fails the update rather than installing a
    // half-parsed relay map the data path would then trust.
    assert(!codec.decode("{\"DERPMap\":{\"Regions\":[]}}", &error));
    assert(error == "netmap DERPMap.Regions is not an object");
    assert(!codec.decode("{\"DERPMap\":[]}", &error));
    assert(error == "netmap DERPMap is not an object");

    // Official control planes emit PeersChangedPatch frames with partial
    // per-node updates. NodeIDs resolve through the same mapping as
    // removals; unknown or malformed entries are skipped, never fatal.
    MapCodec patchCodec;
    const auto patchFull = patchCodec.decode(
        "{\"Peers\":[{\"ID\":7,\"StableID\":\"gaming-pc\",\"Key\":\"nodekey:" +
            zeroKey + "\"}]}",
        &error);
    assert(patchFull && patchFull->fullPeers->size() == 1);
    const auto patch = patchCodec.decode(
        "{\"PeersChangedPatch\":["
        "{\"NodeID\":7,\"Online\":false,\"LastSeen\":\"2026-09-13T19:32:40Z\"},"
        "{\"NodeID\":777,\"Online\":true},"
        "{\"Online\":true},"
        "\"not-an-object\"]}",
        &error);
    assert(patch);
    assert(patch->delta.onlineChanges.size() == 1);
    assert(patch->delta.onlineChanges.front().stableId == "gaming-pc");
    assert(!patch->delta.onlineChanges.front().online);
    assert(!patch->derpMap.has_value());
    return 0;
}
