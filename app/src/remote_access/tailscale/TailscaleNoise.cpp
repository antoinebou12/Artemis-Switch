#include "TailscaleNoise.hpp"

extern "C" {
#include <blake2s.h>
#include <monocypher.h>
}

#include <algorithm>
#include <array>
#include <cstring>

#if defined(__SWITCH__)
extern "C" {
void tailscale_internal_blake2s_init(blake2s_state*, size_t);
void tailscale_internal_blake2s_update(blake2s_state*, const void*, size_t);
void tailscale_internal_blake2s_final(blake2s_state*, void*);
void tailscale_internal_crypto_aead_init_ietf(crypto_aead_ctx*,
                                              const uint8_t[32],
                                              const uint8_t[12]);
void tailscale_internal_crypto_aead_write(crypto_aead_ctx*, uint8_t*, uint8_t[16],
                                          const uint8_t*, size_t,
                                          const uint8_t*, size_t);
int tailscale_internal_crypto_aead_read(crypto_aead_ctx*, uint8_t*,
                                        const uint8_t[16], const uint8_t*,
                                        size_t, const uint8_t*, size_t);
void tailscale_internal_crypto_wipe(void*, size_t);
void tailscale_internal_crypto_x25519(uint8_t[32], const uint8_t[32],
                                      const uint8_t[32]);
void tailscale_internal_crypto_x25519_public_key(uint8_t[32],
                                                 const uint8_t[32]);
}
#define TS_BLAKE_INIT tailscale_internal_blake2s_init
#define TS_BLAKE_UPDATE tailscale_internal_blake2s_update
#define TS_BLAKE_FINAL tailscale_internal_blake2s_final
#define TS_AEAD_INIT tailscale_internal_crypto_aead_init_ietf
#define TS_AEAD_WRITE tailscale_internal_crypto_aead_write
#define TS_AEAD_READ tailscale_internal_crypto_aead_read
#define TS_WIPE tailscale_internal_crypto_wipe
#define TS_X25519 tailscale_internal_crypto_x25519
#define TS_X25519_PUBLIC tailscale_internal_crypto_x25519_public_key
#else
#define TS_BLAKE_INIT blake2s_init
#define TS_BLAKE_UPDATE blake2s_update
#define TS_BLAKE_FINAL blake2s_final
#define TS_AEAD_INIT crypto_aead_init_ietf
#define TS_AEAD_WRITE crypto_aead_write
#define TS_AEAD_READ crypto_aead_read
#define TS_WIPE crypto_wipe
#define TS_X25519 crypto_x25519
#define TS_X25519_PUBLIC crypto_x25519_public_key
#endif

namespace artemis::tailscale {
namespace {
constexpr std::string_view kProtocolName =
    "Noise_IK_25519_ChaChaPoly_BLAKE2s";
constexpr std::string_view kProloguePrefix = "Tailscale Control Protocol v";
constexpr std::size_t kMaxBufferedCiphertext = 64 * 1024;

Key32 hashParts(std::span<const std::uint8_t> first,
                std::span<const std::uint8_t> second) {
    Key32 output{};
    blake2s_state state;
    TS_BLAKE_INIT(&state, output.size());
    if (!first.empty()) TS_BLAKE_UPDATE(&state, first.data(), first.size());
    if (!second.empty()) TS_BLAKE_UPDATE(&state, second.data(), second.size());
    TS_BLAKE_FINAL(&state, output.data());
    return output;
}

Key32 hmac(std::span<const std::uint8_t> key,
           std::span<const std::uint8_t> data) {
    std::array<std::uint8_t, 64> padded{};
    if (key.size() > padded.size()) {
        const auto reduced = hashParts(key, {});
        std::copy(reduced.begin(), reduced.end(), padded.begin());
    } else {
        std::copy(key.begin(), key.end(), padded.begin());
    }
    std::array<std::uint8_t, 64> innerPad{};
    std::array<std::uint8_t, 64> outerPad{};
    for (std::size_t i = 0; i < padded.size(); ++i) {
        innerPad[i] = padded[i] ^ 0x36U;
        outerPad[i] = padded[i] ^ 0x5cU;
    }
    const auto inner = hashParts(innerPad, data);
    auto output = hashParts(outerPad, inner);
    TS_WIPE(padded.data(), padded.size());
    TS_WIPE(innerPad.data(), innerPad.size());
    TS_WIPE(outerPad.data(), outerPad.size());
    return output;
}

std::array<Key32, 2> hkdf2(const Key32& salt,
                           std::span<const std::uint8_t> input) {
    auto prk = hmac(salt, input);
    constexpr std::array<std::uint8_t, 1> one{1};
    auto first = hmac(prk, one);
    std::array<std::uint8_t, 33> secondInput{};
    std::copy(first.begin(), first.end(), secondInput.begin());
    secondInput.back() = 2;
    auto second = hmac(prk, secondInput);
    TS_WIPE(prk.data(), prk.size());
    return {first, second};
}

std::array<std::uint8_t, 12> nonceBytes(std::uint64_t nonce) {
    std::array<std::uint8_t, 12> bytes{};
    for (int i = 0; i < 8; ++i)
        bytes[11 - i] = static_cast<std::uint8_t>(nonce >> (i * 8));
    return bytes;
}

bool encrypt(const Key32& key, std::uint64_t nonce,
             std::span<const std::uint8_t> additionalData,
             std::span<const std::uint8_t> plaintext,
             std::span<std::uint8_t> ciphertextAndTag) {
    if (ciphertextAndTag.size() != plaintext.size() + 16) return false;
    crypto_aead_ctx context;
    const auto nonceValue = nonceBytes(nonce);
    TS_AEAD_INIT(&context, key.data(), nonceValue.data());
    auto* ciphertext = ciphertextAndTag.data();
    auto* tag = ciphertextAndTag.data() + plaintext.size();
    TS_AEAD_WRITE(&context, ciphertext, tag,
                  additionalData.empty() ? nullptr : additionalData.data(),
                  additionalData.size(),
                  plaintext.empty() ? nullptr : plaintext.data(),
                  plaintext.size());
    TS_WIPE(&context, sizeof(context));
    return true;
}

bool decrypt(const Key32& key, std::uint64_t nonce,
             std::span<const std::uint8_t> additionalData,
             std::span<const std::uint8_t> ciphertextAndTag,
             std::span<std::uint8_t> plaintext) {
    if (ciphertextAndTag.size() != plaintext.size() + 16) return false;
    crypto_aead_ctx context;
    const auto nonceValue = nonceBytes(nonce);
    TS_AEAD_INIT(&context, key.data(), nonceValue.data());
    const auto* tag = ciphertextAndTag.data() + plaintext.size();
    const int result = TS_AEAD_READ(
        &context, plaintext.empty() ? nullptr : plaintext.data(), tag,
        additionalData.empty() ? nullptr : additionalData.data(),
        additionalData.size(), ciphertextAndTag.data(), plaintext.size());
    TS_WIPE(&context, sizeof(context));
    return result == 0;
}

void appendBigEndian16(std::vector<std::uint8_t>& output,
                       std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
    output.push_back(static_cast<std::uint8_t>(value));
}
} // namespace

NoiseClient::NoiseClient(Key32 machinePrivate, Key32 controlPublic,
                         Key32 ephemeralPrivate,
                         std::uint16_t protocolVersion)
    : machinePrivate_(machinePrivate), controlPublic_(controlPublic),
      ephemeralPrivate_(ephemeralPrivate), protocolVersion_(protocolVersion) {}

NoiseClient::~NoiseClient() {
    TS_WIPE(machinePrivate_.data(), machinePrivate_.size());
    TS_WIPE(ephemeralPrivate_.data(), ephemeralPrivate_.size());
    TS_WIPE(chainingKey_.data(), chainingKey_.size());
    TS_WIPE(txKey_.data(), txKey_.size());
    TS_WIPE(rxKey_.data(), rxKey_.size());
}

void NoiseClient::mixHash(std::span<const std::uint8_t> data) {
    hash_ = hashParts(hash_, data);
}

bool NoiseClient::mixDh(const Key32& privateKey, const Key32& publicKey,
                        Key32* cipherKey, std::string* error) {
    Key32 shared{};
    TS_X25519(shared.data(), privateKey.data(), publicKey.data());
    if (std::all_of(shared.begin(), shared.end(),
                    [](std::uint8_t value) { return value == 0; })) {
        TS_WIPE(shared.data(), shared.size());
        fail("invalid TS2021 X25519 peer key", error);
        return false;
    }
    auto derived = hkdf2(chainingKey_, shared);
    chainingKey_ = derived[0];
    if (cipherKey) *cipherKey = derived[1];
    TS_WIPE(shared.data(), shared.size());
    TS_WIPE(derived[0].data(), derived[0].size());
    TS_WIPE(derived[1].data(), derived[1].size());
    return true;
}

bool NoiseClient::begin(std::vector<std::uint8_t>* initiation,
                        std::string* error) {
    if (!initiation || begun_ || protocolVersion_ == 0) {
        fail("invalid TS2021 handshake state", error);
        return false;
    }
    hash_ = hashParts({}, std::span<const std::uint8_t>(
                              reinterpret_cast<const std::uint8_t*>(kProtocolName.data()),
                              kProtocolName.size()));
    chainingKey_ = hash_;
    const std::string prologue =
        std::string(kProloguePrefix) + std::to_string(protocolVersion_);
    mixHash(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(prologue.data()), prologue.size()));
    mixHash(controlPublic_);

    Key32 ephemeralPublic{};
    TS_X25519_PUBLIC(ephemeralPublic.data(), ephemeralPrivate_.data());
    Key32 machinePublic{};
    TS_X25519_PUBLIC(machinePublic.data(), machinePrivate_.data());
    mixHash(ephemeralPublic);

    Key32 cipherKey{};
    if (!mixDh(ephemeralPrivate_, controlPublic_, &cipherKey, error)) return false;
    std::array<std::uint8_t, 48> encryptedMachine{};
    if (!encrypt(cipherKey, 0, hash_, machinePublic, encryptedMachine)) {
        fail("could not encrypt TS2021 machine key", error);
        return false;
    }
    mixHash(encryptedMachine);
    if (!mixDh(machinePrivate_, controlPublic_, &cipherKey, error)) return false;
    std::array<std::uint8_t, 16> tag{};
    if (!encrypt(cipherKey, 0, hash_, {}, tag)) {
        fail("could not authenticate TS2021 initiation", error);
        return false;
    }
    mixHash(tag);

    initiation->clear();
    initiation->reserve(kInitiationSize);
    appendBigEndian16(*initiation, protocolVersion_);
    initiation->push_back(1);
    appendBigEndian16(*initiation, 96);
    initiation->insert(initiation->end(), ephemeralPublic.begin(),
                       ephemeralPublic.end());
    initiation->insert(initiation->end(), encryptedMachine.begin(),
                       encryptedMachine.end());
    initiation->insert(initiation->end(), tag.begin(), tag.end());
    TS_WIPE(cipherKey.data(), cipherKey.size());
    TS_WIPE(machinePublic.data(), machinePublic.size());
    begun_ = true;
    return initiation->size() == kInitiationSize;
}

bool NoiseClient::complete(std::span<const std::uint8_t> response,
                           std::string* error) {
    if (!begun_ || ready_ || response.size() != kResponseSize ||
        response[0] != 2 || response[1] != 0 || response[2] != 48) {
        fail("invalid TS2021 handshake response", error);
        return false;
    }
    Key32 controlEphemeral{};
    std::copy_n(response.begin() + 3, controlEphemeral.size(),
                controlEphemeral.begin());
    mixHash(controlEphemeral);
    if (!mixDh(ephemeralPrivate_, controlEphemeral, nullptr, error)) return false;
    Key32 cipherKey{};
    if (!mixDh(machinePrivate_, controlEphemeral, &cipherKey, error)) return false;
    std::array<std::uint8_t, 0> empty{};
    if (!decrypt(cipherKey, 0, hash_, response.subspan(35, 16), empty)) {
        fail("TS2021 response authentication failed", error);
        return false;
    }
    mixHash(response.subspan(35, 16));
    split();
    TS_WIPE(cipherKey.data(), cipherKey.size());
    ready_ = true;
    return true;
}

void NoiseClient::split() {
    const auto keys = hkdf2(chainingKey_, {});
    txKey_ = keys[0];
    rxKey_ = keys[1];
    txNonce_ = 0;
    rxNonce_ = 0;
}

bool NoiseClient::frame(std::span<const std::uint8_t> plaintext,
                        std::vector<std::uint8_t>* framed,
                        std::string* error) {
    if (!ready_ || !framed) {
        fail("TS2021 session is not ready", error);
        return false;
    }
    framed->clear();
    std::size_t offset = 0;
    while (offset < plaintext.size()) {
        const auto length = std::min(kMaxPlaintextSize, plaintext.size() - offset);
        const auto ciphertextLength = length + 16;
        framed->push_back(4);
        appendBigEndian16(*framed,
                          static_cast<std::uint16_t>(ciphertextLength));
        const auto outputOffset = framed->size();
        framed->resize(outputOffset + ciphertextLength);
        if (!encrypt(txKey_, txNonce_++, {}, plaintext.subspan(offset, length),
                     std::span<std::uint8_t>(*framed).subspan(
                         outputOffset, ciphertextLength))) {
            fail("TS2021 record encryption failed", error);
            return false;
        }
        offset += length;
    }
    return true;
}

bool NoiseClient::consume(
    std::span<const std::uint8_t> bytes,
    std::vector<std::vector<std::uint8_t>>* plaintext,
    std::string* error) {
    if (!ready_ || !plaintext ||
        receiveBuffer_.size() + bytes.size() > kMaxBufferedCiphertext) {
        fail("invalid or oversized TS2021 receive buffer", error);
        return false;
    }
    receiveBuffer_.insert(receiveBuffer_.end(), bytes.begin(), bytes.end());
    std::size_t offset = 0;
    while (receiveBuffer_.size() - offset >= 3) {
        if (receiveBuffer_[offset] != 4) {
            fail("unexpected TS2021 record type", error);
            return false;
        }
        const std::size_t ciphertextLength =
            static_cast<std::size_t>(receiveBuffer_[offset + 1]) << 8U |
            receiveBuffer_[offset + 2];
        if (ciphertextLength < 16 || ciphertextLength > kMaxRecordSize - 3) {
            fail("invalid TS2021 record length", error);
            return false;
        }
        if (receiveBuffer_.size() - offset < 3 + ciphertextLength) break;
        std::vector<std::uint8_t> decoded(ciphertextLength - 16);
        const auto ciphertext = std::span<const std::uint8_t>(receiveBuffer_)
                                    .subspan(offset + 3, ciphertextLength);
        if (!decrypt(rxKey_, rxNonce_++, {}, ciphertext, decoded)) {
            fail("TS2021 record authentication failed", error);
            return false;
        }
        plaintext->push_back(std::move(decoded));
        offset += 3 + ciphertextLength;
    }
    if (offset != 0)
        receiveBuffer_.erase(receiveBuffer_.begin(),
                             receiveBuffer_.begin() + offset);
    return true;
}

void NoiseClient::fail(std::string_view message, std::string* error) {
    if (error) *error = std::string(message);
    if (begun_) ready_ = false;
}

} // namespace artemis::tailscale
