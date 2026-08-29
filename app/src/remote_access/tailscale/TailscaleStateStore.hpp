#pragma once

#include "TailscaleTypes.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace artemis::tailscale {

enum class StateProtection : std::uint8_t { Plain = 0, Passphrase = 1 };

struct StateKdfParameters {
    std::uint32_t memoryKiB = 16 * 1024;
    std::uint32_t passes = 3;
    std::uint32_t lanes = 1;
};

class SecureBytes {
public:
    SecureBytes() = default;
    explicit SecureBytes(std::string_view value);
    ~SecureBytes();
    SecureBytes(const SecureBytes&) = delete;
    SecureBytes& operator=(const SecureBytes&) = delete;
    SecureBytes(SecureBytes&& other) noexcept;
    SecureBytes& operator=(SecureBytes&& other) noexcept;

    [[nodiscard]] std::span<const std::uint8_t> view() const noexcept;
    void clear() noexcept;

private:
    std::vector<std::uint8_t> bytes_;
};

class StateStore {
public:
    explicit StateStore(std::filesystem::path path);

    bool save(const Identity& identity, StateProtection protection,
              std::span<const std::uint8_t> passphrase,
              const StateKdfParameters& parameters = {},
              std::string* error = nullptr) const;
    std::optional<Identity> load(std::span<const std::uint8_t> passphrase,
                                 std::string* error = nullptr) const;
    [[nodiscard]] std::optional<StateProtection> protection(
        std::string* error = nullptr) const;
    bool remove(std::string* error = nullptr) const;

private:
    std::filesystem::path path_;
};

} // namespace artemis::tailscale
