#include "TailscaleStateStore.hpp"

extern "C" {
#include <monocypher.h>
}

#if defined(__SWITCH__)
extern "C" {
void tailscale_internal_crypto_argon2(uint8_t*, uint32_t, void*,
                                      crypto_argon2_config,
                                      crypto_argon2_inputs,
                                      crypto_argon2_extras);
extern const crypto_argon2_extras tailscale_internal_crypto_argon2_no_extras;
void tailscale_internal_crypto_wipe(void*, size_t);
void tailscale_internal_crypto_aead_lock(
    uint8_t*, uint8_t[16], const uint8_t[32], const uint8_t[24],
    const uint8_t*, size_t, const uint8_t*, size_t);
int tailscale_internal_crypto_aead_unlock(
    uint8_t*, const uint8_t[16], const uint8_t[32], const uint8_t[24],
    const uint8_t*, size_t, const uint8_t*, size_t);
void tailscale_internal_crypto_blake2b(uint8_t*, size_t, const uint8_t*,
                                       size_t);
int tailscale_internal_crypto_verify16(const uint8_t[16], const uint8_t[16]);
}
#define TS_STATE_ARGON2 tailscale_internal_crypto_argon2
#define TS_STATE_ARGON2_EXTRAS tailscale_internal_crypto_argon2_no_extras
#define TS_STATE_WIPE tailscale_internal_crypto_wipe
#define TS_STATE_LOCK tailscale_internal_crypto_aead_lock
#define TS_STATE_UNLOCK tailscale_internal_crypto_aead_unlock
#define TS_STATE_BLAKE2B tailscale_internal_crypto_blake2b
#define TS_STATE_VERIFY16 tailscale_internal_crypto_verify16
#else
#define TS_STATE_ARGON2 crypto_argon2
#define TS_STATE_ARGON2_EXTRAS crypto_argon2_no_extras
#define TS_STATE_WIPE crypto_wipe
#define TS_STATE_LOCK crypto_aead_lock
#define TS_STATE_UNLOCK crypto_aead_unlock
#define TS_STATE_BLAKE2B crypto_blake2b
#define TS_STATE_VERIFY16 crypto_verify16
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <random>

#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace artemis::tailscale {
namespace {

constexpr std::array<std::uint8_t, 8> kMagic{'A', 'R', 'T', 'T', 'S', '0', '1', 0};
constexpr std::uint16_t kVersion = 1;
constexpr std::size_t kIdentitySize = 96;
constexpr std::uint32_t kMinMemoryKiB = 8 * 1024;
constexpr std::uint32_t kMaxMemoryKiB = 32 * 1024;

void append16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void append32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (unsigned int shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint16_t read16(std::span<const std::uint8_t> bytes) {
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(bytes[1]) << 8U;
}

std::uint32_t read32(std::span<const std::uint8_t> bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           static_cast<std::uint32_t>(bytes[1]) << 8U |
           static_cast<std::uint32_t>(bytes[2]) << 16U |
           static_cast<std::uint32_t>(bytes[3]) << 24U;
}

void randomBytes(std::span<std::uint8_t> output) {
#if defined(__SWITCH__)
    randomGet(output.data(), output.size());
#else
    std::random_device source;
    for (auto& byte : output)
        byte = static_cast<std::uint8_t>(source());
#endif
}

std::array<std::uint8_t, kIdentitySize> serialize(const Identity& identity) {
    std::array<std::uint8_t, kIdentitySize> bytes{};
    std::copy(identity.machinePrivate.begin(), identity.machinePrivate.end(),
              bytes.begin());
    std::copy(identity.nodePrivate.begin(), identity.nodePrivate.end(),
              bytes.begin() + 32);
    std::copy(identity.discoPrivate.begin(), identity.discoPrivate.end(),
              bytes.begin() + 64);
    return bytes;
}

Identity deserialize(std::span<const std::uint8_t, kIdentitySize> bytes) {
    Identity identity;
    std::copy_n(bytes.begin(), 32, identity.machinePrivate.begin());
    std::copy_n(bytes.begin() + 32, 32, identity.nodePrivate.begin());
    std::copy_n(bytes.begin() + 64, 32, identity.discoPrivate.begin());
    return identity;
}

bool validateKdf(const StateKdfParameters& parameters, std::string* error) {
    if (parameters.memoryKiB < kMinMemoryKiB ||
        parameters.memoryKiB > kMaxMemoryKiB || parameters.passes == 0 ||
        parameters.passes > 10 || parameters.lanes != 1) {
        if (error) *error = "unsupported state KDF parameters";
        return false;
    }
    return true;
}

bool deriveKey(std::array<std::uint8_t, 32>& key,
               std::span<const std::uint8_t> passphrase,
               std::span<const std::uint8_t, 16> salt,
               const StateKdfParameters& parameters, std::string* error) {
    if (passphrase.empty()) {
        if (error) *error = "passphrase is required";
        return false;
    }
    if (!validateKdf(parameters, error))
        return false;
    std::vector<std::uint8_t> work(
        static_cast<std::size_t>(parameters.memoryKiB) * 1024U);
    const crypto_argon2_config config{CRYPTO_ARGON2_ID, parameters.memoryKiB,
                                      parameters.passes, parameters.lanes};
    const crypto_argon2_inputs inputs{
        passphrase.data(), salt.data(),
        static_cast<std::uint32_t>(passphrase.size()),
        static_cast<std::uint32_t>(salt.size())};
    TS_STATE_ARGON2(key.data(), static_cast<std::uint32_t>(key.size()),
                    work.data(), config, inputs, TS_STATE_ARGON2_EXTRAS);
    TS_STATE_WIPE(work.data(), work.size());
    return true;
}

struct ParsedHeader {
    StateProtection protection = StateProtection::Plain;
    StateKdfParameters kdf;
    std::array<std::uint8_t, 16> salt{};
    std::array<std::uint8_t, 24> nonce{};
    std::uint32_t payloadSize = 0;
    std::size_t payloadOffset = 0;
};

std::optional<ParsedHeader> parseHeader(std::span<const std::uint8_t> bytes,
                                        std::string* error) {
    constexpr std::size_t headerSize = 8 + 2 + 2 + 12 + 16 + 24 + 4;
    if (bytes.size() < headerSize + 16 ||
        !std::equal(kMagic.begin(), kMagic.end(), bytes.begin()) ||
        read16(bytes.subspan(8, 2)) != kVersion) {
        if (error) *error = "invalid Tailscale state header";
        return std::nullopt;
    }
    ParsedHeader header;
    const auto protection = bytes[10];
    if (protection > static_cast<std::uint8_t>(StateProtection::Passphrase)) {
        if (error) *error = "unknown Tailscale state protection";
        return std::nullopt;
    }
    header.protection = static_cast<StateProtection>(protection);
    header.kdf.memoryKiB = read32(bytes.subspan(12, 4));
    header.kdf.passes = read32(bytes.subspan(16, 4));
    header.kdf.lanes = read32(bytes.subspan(20, 4));
    std::copy_n(bytes.begin() + 24, 16, header.salt.begin());
    std::copy_n(bytes.begin() + 40, 24, header.nonce.begin());
    header.payloadSize = read32(bytes.subspan(64, 4));
    header.payloadOffset = headerSize;
    if (header.payloadSize != kIdentitySize ||
        bytes.size() != header.payloadOffset + header.payloadSize + 16) {
        if (error) *error = "invalid Tailscale state size";
        return std::nullopt;
    }
    return header;
}

} // namespace

SecureBytes::SecureBytes(std::string_view value)
    : bytes_(value.begin(), value.end()) {}

SecureBytes::~SecureBytes() { clear(); }

SecureBytes::SecureBytes(SecureBytes&& other) noexcept
    : bytes_(std::move(other.bytes_)) {}

SecureBytes& SecureBytes::operator=(SecureBytes&& other) noexcept {
    if (this != &other) {
        clear();
        bytes_ = std::move(other.bytes_);
    }
    return *this;
}

std::span<const std::uint8_t> SecureBytes::view() const noexcept {
    return bytes_;
}

void SecureBytes::clear() noexcept {
    if (!bytes_.empty())
        TS_STATE_WIPE(bytes_.data(), bytes_.size());
    bytes_.clear();
    bytes_.shrink_to_fit();
}

StateStore::StateStore(std::filesystem::path path) : path_(std::move(path)) {}

bool StateStore::save(const Identity& identity, StateProtection protection,
                      std::span<const std::uint8_t> passphrase,
                      const StateKdfParameters& parameters,
                      std::string* error) const {
    if (protection == StateProtection::Passphrase &&
        !validateKdf(parameters, error))
        return false;

    auto payload = serialize(identity);
    std::array<std::uint8_t, 16> salt{};
    std::array<std::uint8_t, 24> nonce{};
    randomBytes(salt);
    randomBytes(nonce);

    std::vector<std::uint8_t> output;
    output.reserve(68 + payload.size() + 16);
    output.insert(output.end(), kMagic.begin(), kMagic.end());
    append16(output, kVersion);
    output.push_back(static_cast<std::uint8_t>(protection));
    output.push_back(0);
    append32(output, parameters.memoryKiB);
    append32(output, parameters.passes);
    append32(output, parameters.lanes);
    output.insert(output.end(), salt.begin(), salt.end());
    output.insert(output.end(), nonce.begin(), nonce.end());
    append32(output, static_cast<std::uint32_t>(payload.size()));

    std::array<std::uint8_t, 16> tag{};
    if (protection == StateProtection::Passphrase) {
        std::array<std::uint8_t, 32> key{};
        if (!deriveKey(key, passphrase, salt, parameters, error)) {
            TS_STATE_WIPE(payload.data(), payload.size());
            return false;
        }
        const auto header = std::span<const std::uint8_t>(output);
        const auto offset = output.size();
        output.resize(offset + payload.size());
        TS_STATE_LOCK(output.data() + offset, tag.data(), key.data(),
                         nonce.data(), header.data(), header.size(),
                         payload.data(), payload.size());
        TS_STATE_WIPE(key.data(), key.size());
    } else {
        TS_STATE_BLAKE2B(tag.data(), tag.size(), payload.data(), payload.size());
        output.insert(output.end(), payload.begin(), payload.end());
    }
    output.insert(output.end(), tag.begin(), tag.end());
    TS_STATE_WIPE(payload.data(), payload.size());

    std::error_code filesystemError;
    std::filesystem::create_directories(path_.parent_path(), filesystemError);
    if (filesystemError) {
        if (error) *error = "cannot create Tailscale state directory";
        TS_STATE_WIPE(output.data(), output.size());
        return false;
    }
    const auto temporary = std::filesystem::path(path_.string() + ".tmp");
    const auto backup = std::filesystem::path(path_.string() + ".bak");
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file || !file.write(reinterpret_cast<const char*>(output.data()),
                                 static_cast<std::streamsize>(output.size()))) {
            if (error) *error = "cannot write Tailscale state";
            TS_STATE_WIPE(output.data(), output.size());
            return false;
        }
    }
    if (std::filesystem::exists(path_)) {
        std::filesystem::remove(backup, filesystemError);
        filesystemError.clear();
        std::filesystem::rename(path_, backup, filesystemError);
        if (filesystemError) {
            if (error) *error = "cannot back up Tailscale state";
            TS_STATE_WIPE(output.data(), output.size());
            return false;
        }
    }
    filesystemError.clear();
    std::filesystem::rename(temporary, path_, filesystemError);
    if (filesystemError) {
        std::error_code ignored;
        if (std::filesystem::exists(backup))
            std::filesystem::rename(backup, path_, ignored);
        if (error) *error = "cannot replace Tailscale state";
        TS_STATE_WIPE(output.data(), output.size());
        return false;
    }
    std::filesystem::remove(backup, filesystemError);
    TS_STATE_WIPE(output.data(), output.size());
    return true;
}

std::optional<Identity> StateStore::load(
    std::span<const std::uint8_t> passphrase, std::string* error) const {
    const auto loadFile = [&](const std::filesystem::path& candidate,
                              std::string* candidateError)
        -> std::optional<Identity> {
        std::ifstream file(candidate, std::ios::binary | std::ios::ate);
        if (!file) {
            if (candidateError) *candidateError = "Tailscale state does not exist";
            return std::nullopt;
        }
        const auto length = file.tellg();
        if (length <= 0 || length > 4096) {
            if (candidateError) *candidateError = "invalid Tailscale state size";
            return std::nullopt;
        }
        file.seekg(0);
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
        if (!file.read(reinterpret_cast<char*>(bytes.data()), length)) {
            TS_STATE_WIPE(bytes.data(), bytes.size());
            if (candidateError) *candidateError = "cannot read Tailscale state";
            return std::nullopt;
        }
        const auto header = parseHeader(bytes, candidateError);
        if (!header) {
            TS_STATE_WIPE(bytes.data(), bytes.size());
            return std::nullopt;
        }
        std::array<std::uint8_t, kIdentitySize> payload{};
        const auto ciphertext = std::span<const std::uint8_t>(bytes).subspan(
            header->payloadOffset, header->payloadSize);
        const auto tag = std::span<const std::uint8_t, 16>(
            bytes.data() + header->payloadOffset + header->payloadSize, 16);
        bool valid = false;
        if (header->protection == StateProtection::Passphrase) {
            std::array<std::uint8_t, 32> key{};
            if (deriveKey(key, passphrase, header->salt, header->kdf,
                          candidateError)) {
                valid = TS_STATE_UNLOCK(
                            payload.data(), tag.data(), key.data(),
                            header->nonce.data(), bytes.data(),
                            header->payloadOffset, ciphertext.data(),
                            ciphertext.size()) == 0;
            }
            TS_STATE_WIPE(key.data(), key.size());
        } else {
            std::array<std::uint8_t, 16> expected{};
            TS_STATE_BLAKE2B(expected.data(), expected.size(), ciphertext.data(),
                           ciphertext.size());
            valid = TS_STATE_VERIFY16(expected.data(), tag.data()) == 0;
            if (valid)
                std::copy(ciphertext.begin(), ciphertext.end(), payload.begin());
            TS_STATE_WIPE(expected.data(), expected.size());
        }
        TS_STATE_WIPE(bytes.data(), bytes.size());
        if (!valid) {
            TS_STATE_WIPE(payload.data(), payload.size());
            if (candidateError)
                *candidateError = "Tailscale state authentication failed";
            return std::nullopt;
        }
        const auto identity = deserialize(payload);
        TS_STATE_WIPE(payload.data(), payload.size());
        return identity;
    };

    std::string primaryError;
    if (auto identity = loadFile(path_, &primaryError))
        return identity;

    // A crash between renaming the old file to .bak and installing .tmp can
    // leave no primary state. A valid backup is authoritative recovery data;
    // never accept an unauthenticated temporary file.
    const auto backup = std::filesystem::path(path_.string() + ".bak");
    std::error_code filesystemError;
    if (std::filesystem::exists(backup, filesystemError) && !filesystemError) {
        std::string backupError;
        if (auto identity = loadFile(backup, &backupError)) {
            if (!std::filesystem::exists(path_, filesystemError)) {
                filesystemError.clear();
                std::filesystem::rename(backup, path_, filesystemError);
            }
            if (error) error->clear();
            return identity;
        }
        if (error)
            *error = primaryError + "; backup recovery failed: " + backupError;
        return std::nullopt;
    }

    if (error) *error = primaryError;
    return std::nullopt;
}

std::optional<StateProtection> StateStore::protection(
    std::string* error) const {
    std::ifstream file(path_, std::ios::binary);
    std::array<std::uint8_t, 12> prefix{};
    if (!file.read(reinterpret_cast<char*>(prefix.data()), prefix.size())) {
        if (error) *error = "cannot read Tailscale state header";
        return std::nullopt;
    }
    if (!std::equal(kMagic.begin(), kMagic.end(), prefix.begin()) ||
        read16(std::span<const std::uint8_t>(prefix).subspan(8, 2)) != kVersion ||
        prefix[10] > static_cast<std::uint8_t>(StateProtection::Passphrase)) {
        if (error) *error = "invalid Tailscale state header";
        return std::nullopt;
    }
    return static_cast<StateProtection>(prefix[10]);
}

bool StateStore::remove(std::string* error) const {
    std::error_code filesystemError;
    std::filesystem::remove(path_, filesystemError);
    std::filesystem::remove(std::filesystem::path(path_.string() + ".tmp"),
                            filesystemError);
    std::filesystem::remove(std::filesystem::path(path_.string() + ".bak"),
                            filesystemError);
    if (filesystemError) {
        if (error) *error = "cannot remove Tailscale state";
        return false;
    }
    return true;
}

} // namespace artemis::tailscale
