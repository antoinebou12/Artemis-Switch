#include "TailscaleNoise.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
#include <monocypher.h>
}

using artemis::tailscale::Key32;
using artemis::tailscale::NoiseClient;

namespace {
Key32 sequential(std::uint8_t start) {
    Key32 key{};
    for (std::size_t i = 0; i < key.size(); ++i)
        key[i] = static_cast<std::uint8_t>(start + i);
    return key;
}

std::vector<std::uint8_t> fromHex(std::string_view hex) {
    auto nibble = [](char value) -> std::uint8_t {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        return 0xff;
    };
    assert(hex.size() % 2 == 0);
    std::vector<std::uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const auto high = nibble(hex[i]);
        const auto low = nibble(hex[i + 1]);
        assert(high != 0xff && low != 0xff);
        bytes.push_back(static_cast<std::uint8_t>((high << 4U) | low));
    }
    return bytes;
}
} // namespace

int main() {
    const auto machinePrivate = sequential(1);
    const auto controlPrivate = sequential(65);
    const auto ephemeralPrivate = sequential(129);
    Key32 controlPublic{};
    crypto_x25519_public_key(controlPublic.data(), controlPrivate.data());

    NoiseClient first(machinePrivate, controlPublic, ephemeralPrivate);
    std::vector<std::uint8_t> initiation;
    std::string error;
    assert(first.begin(&initiation, &error));
    assert(error.empty());
    assert(initiation.size() == NoiseClient::kInitiationSize);
    assert(initiation[0] == 0 && initiation[1] == 1);
    assert(initiation[2] == 1);
    assert(initiation[3] == 0 && initiation[4] == 96);
    // Independent known-answer value generated with Python cryptography's
    // X25519/ChaCha20Poly1305 and hashlib's BLAKE2s/HMAC implementation.
    const auto expected = fromHex(
        "0001010060883186b800b41d5cf0429695da9b3cc4f328ebcd184a6e482fa578"
        "c103f06c777e515e2649e7bc3053b2ad63f8f50b185b3f445b2075edc5928e5"
        "29b876fcf979321cb54bff8016e977eb62fd68397aada5048f147578772dd9cf9"
        "1f4675d0d4");
    assert(initiation == expected);

    // The same fixed keys must produce exactly the same Noise initiation.
    NoiseClient second(machinePrivate, controlPublic, ephemeralPrivate);
    std::vector<std::uint8_t> duplicate;
    assert(second.begin(&duplicate, &error));
    assert(duplicate == initiation);

    std::array<std::uint8_t, NoiseClient::kResponseSize> malformed{};
    malformed[0] = 2;
    malformed[2] = 48;
    assert(!first.complete(malformed, &error));
    assert(!error.empty());

    std::vector<std::uint8_t> framed;
    assert(!first.frame(std::array<std::uint8_t, 1>{1}, &framed, &error));

    NoiseClient verified(machinePrivate, controlPublic, ephemeralPrivate);
    assert(verified.begin(&duplicate, &error));
    const auto response = fromHex(
        "0200303a553d74792d727efa9b9a4cde3da1ad93f1a2d0c09cb639b1a3c0fda"
        "14cbe24db87199b1f27ef8d70dfb4bab19590c6");
    assert(verified.complete(response, &error));
    assert(verified.ready());
    const auto expectedHash = fromHex(
        "b26713a873ef3decad270149ae75d6aa2ea161a14975917e99fbf11bb9c567da");
    assert(std::equal(expectedHash.begin(), expectedHash.end(),
                      verified.handshakeHash().begin()));

    const std::string clientMessage = "client>server";
    assert(verified.frame(
        std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(clientMessage.data()),
            clientMessage.size()),
        &framed, &error));
    assert(framed == fromHex(
        "04001db102e0b26c296ea91cdc6ece50431abcdecc1956c04cb68a0aaca389a2"));

    const auto serverRecord = fromHex(
        "04001d238cc3c32430135bc534314ce837e7b69fc6667e593c0ff7cbdf4081ea");
    std::vector<std::vector<std::uint8_t>> decoded;
    assert(verified.consume(std::span(serverRecord).first(7), &decoded, &error));
    assert(decoded.empty());
    assert(verified.consume(std::span(serverRecord).subspan(7), &decoded,
                            &error));
    assert(decoded.size() == 1);
    assert(std::string(decoded[0].begin(), decoded[0].end()) ==
           "server>client");
    return 0;
}
