#include "../app/src/remote_access/tailscale/TailscaleProtocol.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

using namespace artemis::tailscale;

int main() {
    const std::array<std::uint8_t, 3> payload{1, 2, 3};
    const auto encoded = encodeDerpFrame(0x04, payload);
    assert(encoded.size() == 8);
    DerpFrameDecoder decoder;
    assert(decoder.append(std::span(encoded).first(3)));
    assert(!decoder.take());
    assert(decoder.append(std::span(encoded).subspan(3)));
    const auto frame = decoder.take();
    assert(frame && frame->type == 0x04 && frame->payload.size() == 3);

    std::vector<std::uint8_t> oversized{0x04, 0x00, 0x01, 0x00, 0x01};
    assert(decoder.append(oversized));
    std::string error;
    assert(!decoder.take(&error));
    assert(!error.empty());

    StunTransaction transaction;
    for (std::size_t i = 0; i < transaction.id.size(); ++i)
        transaction.id[i] = static_cast<std::uint8_t>(i + 1);
    const auto request = makeStunBindingRequest(transaction);
    assert(request[0] == 0 && request[1] == 1 && request[4] == 0x21);

    // Binding success with XOR-MAPPED-ADDRESS 203.0.113.7:41641.
    std::vector<std::uint8_t> response(32, 0);
    response[0] = 0x01;
    response[1] = 0x01;
    response[3] = 12;
    response[4] = 0x21;
    response[5] = 0x12;
    response[6] = 0xa4;
    response[7] = 0x42;
    std::copy(transaction.id.begin(), transaction.id.end(), response.begin() + 8);
    response[20] = 0x00;
    response[21] = 0x20;
    response[23] = 8;
    response[25] = 0x01;
    const std::uint16_t xport = 41641U ^ 0x2112U;
    response[26] = static_cast<std::uint8_t>(xport >> 8U);
    response[27] = static_cast<std::uint8_t>(xport);
    const std::uint32_t xaddr = 0xcb007107U ^ 0x2112a442U;
    response[28] = static_cast<std::uint8_t>(xaddr >> 24U);
    response[29] = static_cast<std::uint8_t>(xaddr >> 16U);
    response[30] = static_cast<std::uint8_t>(xaddr >> 8U);
    response[31] = static_cast<std::uint8_t>(xaddr);
    const auto endpoint = parseStunBindingResponse(response, transaction, &error);
    assert(endpoint && endpoint->address == "203.0.113.7" &&
           endpoint->port == 41641);

    auto mismatched = response;
    mismatched[8] ^= 1;
    assert(!parseStunBindingResponse(mismatched, transaction, &error));
    assert(error == "STUN transaction mismatch");

    auto badLength = response;
    badLength[3] = 11;
    assert(!parseStunBindingResponse(badLength, transaction, &error));

    DiscoMessage ping;
    ping.type = DiscoType::Ping;
    ping.hasNodeKey = true;
    ping.transaction[0] = 9;
    ping.nodeKey[0] = 7;
    const auto disco = encodeDiscoPayload(ping);
    const auto parsed = parseDiscoPayload(disco, &error);
    assert(parsed && parsed->type == DiscoType::Ping && parsed->hasNodeKey);
    assert(parsed->transaction[0] == 9 && parsed->nodeKey[0] == 7);

    std::array<std::uint8_t, 62> wrapper{};
    const std::array<std::uint8_t, 6> magic{0x54, 0x53, 0xf0,
                                            0x9f, 0x92, 0xac};
    std::copy(magic.begin(), magic.end(), wrapper.begin());
    assert(looksLikeDiscoWrapper(wrapper));
    return 0;
}
