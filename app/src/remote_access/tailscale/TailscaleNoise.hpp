#pragma once

#include "TailscaleTypes.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace artemis::tailscale {

// Client half of Tailscale's TS2021 Noise IK base transport. Network and
// HTTP/2 ownership intentionally stay outside this class.
class NoiseClient {
public:
    static constexpr std::uint16_t kProtocolVersion = 1;
    static constexpr std::size_t kInitiationSize = 101;
    static constexpr std::size_t kResponseSize = 51;
    static constexpr std::size_t kMaxRecordSize = 4096;
    static constexpr std::size_t kMaxPlaintextSize = 4077;

    NoiseClient(Key32 machinePrivate, Key32 controlPublic,
                Key32 ephemeralPrivate,
                std::uint16_t protocolVersion = kProtocolVersion);
    ~NoiseClient();
    NoiseClient(const NoiseClient&) = delete;
    NoiseClient& operator=(const NoiseClient&) = delete;

    bool begin(std::vector<std::uint8_t>* initiation,
               std::string* error = nullptr);
    bool complete(std::span<const std::uint8_t> response,
                  std::string* error = nullptr);
    [[nodiscard]] bool ready() const noexcept { return ready_; }
    [[nodiscard]] Key32 handshakeHash() const noexcept { return hash_; }

    bool frame(std::span<const std::uint8_t> plaintext,
               std::vector<std::uint8_t>* framed,
               std::string* error = nullptr);
    bool consume(std::span<const std::uint8_t> bytes,
                 std::vector<std::vector<std::uint8_t>>* plaintext,
                 std::string* error = nullptr);

private:
    bool mixDh(const Key32& privateKey, const Key32& publicKey,
               Key32* cipherKey, std::string* error);
    void mixHash(std::span<const std::uint8_t> data);
    void split();
    void fail(std::string_view message, std::string* error);

    Key32 machinePrivate_{};
    Key32 controlPublic_{};
    Key32 ephemeralPrivate_{};
    Key32 chainingKey_{};
    Key32 hash_{};
    Key32 txKey_{};
    Key32 rxKey_{};
    std::uint16_t protocolVersion_ = kProtocolVersion;
    std::uint64_t txNonce_ = 0;
    std::uint64_t rxNonce_ = 0;
    bool begun_ = false;
    bool ready_ = false;
    std::vector<std::uint8_t> receiveBuffer_;
};

} // namespace artemis::tailscale
