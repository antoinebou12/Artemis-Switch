#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace artemis::tailscale {

// NaCl-compatible crypto_box (XSalsa20-Poly1305): seal and open a message
// for `theirPublic` authenticating as `myPrivate` with a 24-byte nonce.
//
// Wire format matches libsodium (ciphertext = 16-byte MAC || encrypted
// message) and is verified against libsodium known-answer vectors in
// tailscale_box_test, so a DERP server running Go's x/crypto/nacl/box
// accepts what this seals and vice versa.
//
// Construction: X25519 ECDH, HSalsa20 key derivation, XSalsa20 stream with
// the leading 32 keystream bytes reserved as the Poly1305 one-time key
// (NaCl's 32-byte zero prefix). X25519 and Poly1305 come from monocypher;
// only the Salsa20 core is implemented here.
using BoxKey = std::array<std::uint8_t, 32>;
inline constexpr std::size_t kBoxNonceLen = 24;
inline constexpr std::size_t kBoxMacLen = 16;

bool boxSeal(std::span<const std::uint8_t> plainText, const BoxKey& myPrivate,
             const BoxKey& theirPublic,
             std::span<const std::uint8_t, kBoxNonceLen> nonce,
             std::vector<std::uint8_t>& cipherTextOut,
             std::string* error = nullptr);
bool boxOpen(std::span<const std::uint8_t> cipherText, const BoxKey& myPrivate,
             const BoxKey& theirPublic,
             std::span<const std::uint8_t, kBoxNonceLen> nonce,
             std::vector<std::uint8_t>& plainTextOut,
             std::string* error = nullptr);

} // namespace artemis::tailscale
