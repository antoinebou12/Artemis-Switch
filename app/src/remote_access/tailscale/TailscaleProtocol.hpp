#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace artemis::tailscale {

struct DerpFrame {
    std::uint8_t type = 0;
    std::vector<std::uint8_t> payload;
};

class DerpFrameDecoder {
public:
    static constexpr std::size_t kMaxPayload = 64 * 1024;

    bool append(std::span<const std::uint8_t> bytes, std::string* error = nullptr);
    std::optional<DerpFrame> take(std::string* error = nullptr);
    void reset();

private:
    std::vector<std::uint8_t> buffer_;
};

std::vector<std::uint8_t> encodeDerpFrame(
    std::uint8_t type, std::span<const std::uint8_t> payload);

struct StunTransaction {
    std::array<std::uint8_t, 12> id{};
};

struct StunEndpoint {
    std::string address;
    std::uint16_t port = 0;
};

std::array<std::uint8_t, 20> makeStunBindingRequest(
    const StunTransaction& transaction);
std::optional<StunEndpoint> parseStunBindingResponse(
    std::span<const std::uint8_t> packet,
    const StunTransaction& transaction,
    std::string* error = nullptr);

enum class DiscoType : std::uint8_t { Ping = 1, Pong = 2, CallMeMaybe = 3 };

struct DiscoMessage {
    DiscoType type = DiscoType::Ping;
    std::uint8_t version = 0;
    std::array<std::uint8_t, 12> transaction{};
    std::array<std::uint8_t, 32> nodeKey{};
    bool hasNodeKey = false;
};

bool looksLikeDiscoWrapper(std::span<const std::uint8_t> packet);
std::optional<DiscoMessage> parseDiscoPayload(
    std::span<const std::uint8_t> payload, std::string* error = nullptr);
std::vector<std::uint8_t> encodeDiscoPayload(const DiscoMessage& message);

} // namespace artemis::tailscale
