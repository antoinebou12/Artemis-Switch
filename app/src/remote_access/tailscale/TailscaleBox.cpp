#include "TailscaleBox.hpp"

extern "C" {
#include <monocypher.h>
}

#include <algorithm>
#include <cstring>

#if defined(__SWITCH__)
// The Switch build links crypto through the namespaced wg-nx archive rather
// than monocypher directly (same shim pattern as TailscaleDerp).
extern "C" {
void tailscale_internal_crypto_wipe(void*, size_t);
void tailscale_internal_crypto_x25519(uint8_t[32], const uint8_t[32], const uint8_t[32]);
void tailscale_internal_crypto_poly1305(uint8_t[16], const uint8_t*, size_t, const uint8_t[32]);
int tailscale_internal_crypto_verify16(const uint8_t[16], const uint8_t[16]);
}
#define TS_BOX_WIPE tailscale_internal_crypto_wipe
#define TS_BOX_X25519 tailscale_internal_crypto_x25519
#define TS_BOX_POLY1305 tailscale_internal_crypto_poly1305
#define TS_BOX_VERIFY16 tailscale_internal_crypto_verify16
#else
#define TS_BOX_WIPE crypto_wipe
#define TS_BOX_X25519 crypto_x25519
#define TS_BOX_POLY1305 crypto_poly1305
#define TS_BOX_VERIFY16 crypto_verify16
#endif

namespace artemis::tailscale {
namespace {

// libsodium's crypto_box claims keyed primitives only; the raw Salsa20 core
// below is the standard ECRYPT construction, not a custom cipher.
constexpr std::uint8_t kSigma[16] = {'e', 'x', 'p', 'a', 'n', 'd', ' ', '3',
                                     '2', '-', 'b', 'y', 't', 'e', ' ', 'k'};

std::uint32_t rotl(std::uint32_t value, int shift) {
    return static_cast<std::uint32_t>((value << shift) | (value >> (32 - shift)));
}

std::uint32_t load32le(const std::uint8_t src[4]) {
    return static_cast<std::uint32_t>(src[0]) |
           (static_cast<std::uint32_t>(src[1]) << 8U) |
           (static_cast<std::uint32_t>(src[2]) << 16U) |
           (static_cast<std::uint32_t>(src[3]) << 24U);
}

void store32le(std::uint8_t dst[4], std::uint32_t value) {
    dst[0] = static_cast<std::uint8_t>(value);
    dst[1] = static_cast<std::uint8_t>(value >> 8U);
    dst[2] = static_cast<std::uint8_t>(value >> 16U);
    dst[3] = static_cast<std::uint8_t>(value >> 24U);
}

#define BOX_QR(x, a, b, c, d)                                                  \
    do {                                                                       \
        (x)[b] ^= rotl((x)[a] + (x)[d], 7);                                    \
        (x)[c] ^= rotl((x)[b] + (x)[a], 9);                                    \
        (x)[d] ^= rotl((x)[c] + (x)[b], 13);                                   \
        (x)[a] ^= rotl((x)[d] + (x)[c], 18);                                   \
    } while (0)

// One Salsa20/20 block: 64 bytes out of a 16-byte input, 32-byte key.
void salsa20Block(std::uint8_t out[64], const std::uint8_t in[16],
                  const std::uint8_t key[32]) {
    std::uint32_t x[16];
    x[0] = load32le(kSigma + 0);
    x[1] = load32le(key + 0);
    x[2] = load32le(key + 4);
    x[3] = load32le(key + 8);
    x[4] = load32le(key + 12);
    x[5] = load32le(kSigma + 4);
    x[6] = load32le(in + 0);
    x[7] = load32le(in + 4);
    x[8] = load32le(in + 8);
    x[9] = load32le(in + 12);
    x[10] = load32le(kSigma + 8);
    x[11] = load32le(key + 16);
    x[12] = load32le(key + 20);
    x[13] = load32le(key + 24);
    x[14] = load32le(key + 28);
    x[15] = load32le(kSigma + 12);
    std::uint32_t original[16];
    std::memcpy(original, x, sizeof(x));
    for (int round = 0; round < 10; ++round) {
        BOX_QR(x, 0, 4, 8, 12);
        BOX_QR(x, 5, 9, 13, 1);
        BOX_QR(x, 10, 14, 2, 6);
        BOX_QR(x, 15, 3, 7, 11);
        BOX_QR(x, 0, 1, 2, 3);
        BOX_QR(x, 5, 6, 7, 4);
        BOX_QR(x, 10, 11, 8, 9);
        BOX_QR(x, 15, 12, 13, 14);
    }
    for (int i = 0; i < 16; ++i)
        store32le(out + i * 4, x[i] + original[i]);
}

// HSalsa20: 32 bytes out of a 16-byte nonce and 32-byte key.
void hsalsa20(std::uint8_t out[32], const std::uint8_t nonce[16],
              const std::uint8_t key[32]) {
    std::uint32_t x[16];
    x[0] = load32le(kSigma + 0);
    x[1] = load32le(key + 0);
    x[2] = load32le(key + 4);
    x[3] = load32le(key + 8);
    x[4] = load32le(key + 12);
    x[5] = load32le(kSigma + 4);
    x[6] = load32le(nonce + 0);
    x[7] = load32le(nonce + 4);
    x[8] = load32le(nonce + 8);
    x[9] = load32le(nonce + 12);
    x[10] = load32le(kSigma + 8);
    x[11] = load32le(key + 16);
    x[12] = load32le(key + 20);
    x[13] = load32le(key + 24);
    x[14] = load32le(key + 28);
    x[15] = load32le(kSigma + 12);
    for (int round = 0; round < 10; ++round) {
        BOX_QR(x, 0, 4, 8, 12);
        BOX_QR(x, 5, 9, 13, 1);
        BOX_QR(x, 10, 14, 2, 6);
        BOX_QR(x, 15, 3, 7, 11);
        BOX_QR(x, 0, 1, 2, 3);
        BOX_QR(x, 5, 6, 7, 4);
        BOX_QR(x, 10, 11, 8, 9);
        BOX_QR(x, 15, 12, 13, 14);
    }
    // HSalsa20 output words: 0, 5, 10, 15, 6, 7, 8, 9.
    constexpr int kOut[8] = {0, 5, 10, 15, 6, 7, 8, 9};
    for (int i = 0; i < 8; ++i)
        store32le(out + i * 4, x[kOut[i]]);
}

// XSalsa20 keystream prefix of `length` bytes (counter from block 0).
void xsalsa20Keystream(std::uint8_t* out, std::size_t length,
                       const std::uint8_t nonce[kBoxNonceLen],
                       const std::uint8_t key[32]) {
    std::uint8_t subkey[32];
    hsalsa20(subkey, nonce, key);
    std::uint8_t counterBlock[16] = {0};
    std::memcpy(counterBlock, nonce + 16, 8);
    std::uint64_t block = 0;
    while (length > 0) {
        for (int i = 0; i < 8; ++i)
            counterBlock[8 + i] =
                static_cast<std::uint8_t>(block >> (8 * i));
        std::uint8_t keystream[64];
        salsa20Block(keystream, counterBlock, subkey);
        const std::size_t take = std::min<std::size_t>(length, sizeof(keystream));
        std::memcpy(out, keystream, take);
        out += take;
        length -= take;
        ++block;
        TS_BOX_WIPE(keystream, sizeof(keystream));
    }
    TS_BOX_WIPE(subkey, sizeof(subkey));
    TS_BOX_WIPE(counterBlock, sizeof(counterBlock));
}

// NaCl box precomputation: X25519 ECDH then HSalsa20 with a zero nonce.
bool boxKey(std::uint8_t key[32], const BoxKey& myPrivate,
            const BoxKey& theirPublic, std::string* error) {
    std::uint8_t shared[32];
    TS_BOX_X25519(shared, myPrivate.data(), theirPublic.data());
    const bool sharedZero = std::all_of(shared, shared + sizeof(shared),
                                        [](std::uint8_t b) { return b == 0; });
    if (sharedZero) {
        TS_BOX_WIPE(shared, sizeof(shared));
        if (error) *error = "crypto_box: weak or invalid X25519 shared secret";
        return false;
    }
    constexpr std::uint8_t kZeroNonce[16] = {0};
    hsalsa20(key, kZeroNonce, shared);
    TS_BOX_WIPE(shared, sizeof(shared));
    return true;
}

} // namespace

bool boxSeal(std::span<const std::uint8_t> plainText, const BoxKey& myPrivate,
             const BoxKey& theirPublic,
             std::span<const std::uint8_t, kBoxNonceLen> nonce,
             std::vector<std::uint8_t>& cipherTextOut, std::string* error) {
    std::uint8_t key[32];
    if (!boxKey(key, myPrivate, theirPublic, error))
        return false;
    // NaCl pads 32 zero bytes; the first 32 keystream bytes become the
    // Poly1305 one-time key and never leave the device.
    const std::size_t paddedLen = 32 + plainText.size();
    std::vector<std::uint8_t> stream(paddedLen);
    xsalsa20Keystream(stream.data(), stream.size(), nonce.data(), key);
    TS_BOX_WIPE(key, sizeof(key));
    std::vector<std::uint8_t> padded(paddedLen, 0);
    std::copy(plainText.begin(), plainText.end(), padded.begin() + 32);
    for (std::size_t i = 0; i < paddedLen; ++i)
        padded[i] ^= stream[i];
    std::uint8_t mac[kBoxMacLen];
    TS_BOX_POLY1305(mac, padded.data() + 32, padded.size() - 32, stream.data());
    cipherTextOut.resize(kBoxMacLen + plainText.size());
    std::memcpy(cipherTextOut.data(), mac, kBoxMacLen);
    std::memcpy(cipherTextOut.data() + kBoxMacLen, padded.data() + 32,
                plainText.size());
    TS_BOX_WIPE(mac, sizeof(mac));
    TS_BOX_WIPE(stream.data(), stream.size());
    TS_BOX_WIPE(padded.data(), padded.size());
    return true;
}

bool boxOpen(std::span<const std::uint8_t> cipherText, const BoxKey& myPrivate,
             const BoxKey& theirPublic,
             std::span<const std::uint8_t, kBoxNonceLen> nonce,
             std::vector<std::uint8_t>& plainTextOut, std::string* error) {
    if (cipherText.size() < kBoxMacLen) {
        if (error) *error = "crypto_box: ciphertext shorter than auth tag";
        return false;
    }
    std::uint8_t key[32];
    if (!boxKey(key, myPrivate, theirPublic, error))
        return false;
    const std::size_t cipherLen = cipherText.size() - kBoxMacLen;
    // First 32 keystream bytes are the Poly1305 one-time key.
    std::vector<std::uint8_t> stream(32 + cipherLen);
    xsalsa20Keystream(stream.data(), stream.size(), nonce.data(), key);
    TS_BOX_WIPE(key, sizeof(key));
    // Authenticate before touching the message bytes.
    std::uint8_t expectedMac[kBoxMacLen];
    TS_BOX_POLY1305(expectedMac, cipherText.data() + kBoxMacLen, cipherLen,
                    stream.data());
    const bool macOk =
        TS_BOX_VERIFY16(expectedMac, cipherText.data()) == 0;
    TS_BOX_WIPE(expectedMac, sizeof(expectedMac));
    if (!macOk) {
        TS_BOX_WIPE(stream.data(), stream.size());
        plainTextOut.clear();
        if (error) *error = "crypto_box: ciphertext authentication failed";
        return false;
    }
    plainTextOut.resize(cipherLen);
    for (std::size_t i = 0; i < cipherLen; ++i)
        plainTextOut[i] = cipherText[i + kBoxMacLen] ^ stream[32 + i];
    TS_BOX_WIPE(stream.data(), stream.size());
    return true;
}

} // namespace artemis::tailscale
