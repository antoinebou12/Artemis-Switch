#include "TailscaleBox.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

using artemis::tailscale::BoxKey;
using artemis::tailscale::boxOpen;
using artemis::tailscale::boxSeal;
using artemis::tailscale::kBoxNonceLen;

namespace {

std::vector<std::uint8_t> hex(const std::string& text) {
    assert(text.size() % 2 == 0);
    std::vector<std::uint8_t> out;
    out.reserve(text.size() / 2);
    for (std::size_t i = 0; i < text.size(); i += 2) {
        unsigned int byte = 0;
        assert(std::sscanf(text.c_str() + i, "%2x", &byte) == 1);
        out.push_back(static_cast<std::uint8_t>(byte));
    }
    return out;
}

BoxKey key(const std::string& text) {
    const auto bytes = hex(text);
    assert(bytes.size() == 32);
    BoxKey out{};
    std::copy(bytes.begin(), bytes.end(), out.begin());
    return out;
}

std::array<std::uint8_t, kBoxNonceLen> nonce(const std::string& text) {
    const auto bytes = hex(text);
    assert(bytes.size() == kBoxNonceLen);
    std::array<std::uint8_t, kBoxNonceLen> out{};
    std::copy(bytes.begin(), bytes.end(), out.begin());
    return out;
}

} // namespace

int main() {
    // Known-answer vectors generated with libsodium (PyNaCl) crypto_box:
    // seal and open must match byte-for-byte, since the DERP server runs
    // Go's x/crypto/nacl/box and drops anything else.
    const auto aliceSk = key("000102030405060708090a0b0c0d0e0f"
                             "101112131415161718191a1b1c1d1e1f");
    const auto alicePk = key("8f40c5adb68f25624ae5b214ea767a6e"
                             "c94d829d3d7b5e1ad1ba6f3e2138285f");
    const auto bobSk = key("808182838485868788898a8b8c8d8e8f"
                           "909192939495969798999a9b9c9d9e9f");
    const auto bobPk = key("493e82fc74464a59268817623d2053c5e"
                           "b8e2cc4a988b4fee179ec6b010d531d");

    std::string error;

    // 1. Short message (single Salsa block): seal reproduces libsodium bytes.
    {
        const auto n = nonce("1112131415161718191a1b1c1d1e1f"
                             "202122232425262728");
        const std::string message = "{\"version\":2}";
        const auto expected =
            hex("65417c8ca2aeafa531def2b1eee9cb3e7"
                "c3f4d533fa01bac338fd4721a");
        std::vector<std::uint8_t> sealed;
        assert(boxSeal(std::span<const std::uint8_t>(
                           reinterpret_cast<const std::uint8_t*>(message.data()),
                           message.size()),
                       aliceSk, bobPk, n, sealed, &error));
        assert(sealed == expected);

        std::vector<std::uint8_t> opened;
        assert(boxOpen(sealed, bobSk, alicePk, n, opened, &error));
        assert(opened.size() == message.size());
        assert(std::equal(opened.begin(), opened.end(), message.begin()));
    }

    // 2. 100-byte message (multi-block keystream + multi-block Poly1305).
    {
        const auto n = nonce("5152535455565758595a5b5c5d5e5f60"
                             "6162636465666768");
        const auto message = hex(
            "333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f"
            "505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c"
            "6d6e6f707172737475767778797a7b7c7d7e7f80818283848586878889"
            "8a8b8c8d8e8f90919293949596");
        const auto expected = hex(
            "fa7a9f087cbf4d053c8e69e5714b5bbeed4c2dc8aa34811f98fe6252b9"
            "8d08056eb2181d0ce157bb437b6c41b5674e4dba9bc5fab8eee4057a6d"
            "a284c6f1bf2aa5dd7c9f9def89fa3bf3a151326e3eb91aa63287ca404c"
            "176facd731af9eac75d676dfed0737302c16e529ea1f941e4f642ae056");
        std::vector<std::uint8_t> sealed;
        assert(boxSeal(message, aliceSk, bobPk, n, sealed, &error));
        assert(sealed == expected);

        std::vector<std::uint8_t> opened;
        assert(boxOpen(sealed, bobSk, alicePk, n, opened, &error));
        assert(opened == message);
    }

    // 3. Forgery and wrong-key resistance.
    {
        const auto n = nonce("1112131415161718191a1b1c1d1e1f"
                             "202122232425262728");
        const std::string message = "{\"version\":2}";
        std::vector<std::uint8_t> sealed;
        assert(boxSeal(std::span<const std::uint8_t>(
                           reinterpret_cast<const std::uint8_t*>(message.data()),
                           message.size()),
                       aliceSk, bobPk, n, sealed, &error));

        auto tampered = sealed;
        tampered.back() ^= 0x01;
        std::vector<std::uint8_t> opened;
        assert(!boxOpen(tampered, bobSk, alicePk, n, opened, &error));
        assert(opened.empty());

        // Right peer key, wrong private key.
        assert(!boxOpen(sealed, aliceSk, alicePk, n, opened, &error));

        // Truncated below the MAC.
        const std::span<const std::uint8_t> shortSpan(sealed.data(), 15);
        assert(!boxOpen(shortSpan, bobSk, alicePk, n, opened, &error));
    }

    return 0;
}
