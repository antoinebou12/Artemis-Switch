#include "TailscaleControlKey.hpp"

#include "TailscaleControlCodec.hpp"

#include <borealis/extern/nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <limits>
#include <span>
#include <string>
#include <unordered_map>

namespace artemis::tailscale {
namespace {

using Json = nlohmann::json;

constexpr std::size_t kMaxHeaderSize = 16 * 1024;
constexpr std::size_t kMaxBodySize = 64 * 1024;
constexpr std::size_t kMaxWireSize = kMaxHeaderSize + kMaxBodySize + 16 * 1024;

std::string lower(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](char c) {
        return static_cast<char>(
            std::tolower(static_cast<unsigned char>(c)));
    });
    return result;
}

std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.remove_suffix(1);
    return value;
}

bool parseHexSize(std::string_view text, std::size_t* output) {
    const auto extension = text.find(';');
    text = trim(text.substr(0, extension));
    if (text.empty()) return false;
    std::uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(),
                                        value, 16);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        value > kMaxBodySize)
        return false;
    *output = static_cast<std::size_t>(value);
    return true;
}

std::optional<std::string> decodeChunked(std::string_view wire,
                                         std::string* error) {
    std::string body;
    while (true) {
        const auto lineEnd = wire.find("\r\n");
        if (lineEnd == std::string_view::npos) {
            if (error) *error = "truncated control-key chunk header";
            return std::nullopt;
        }
        std::size_t chunkSize = 0;
        if (!parseHexSize(wire.substr(0, lineEnd), &chunkSize)) {
            if (error) *error = "invalid control-key chunk size";
            return std::nullopt;
        }
        wire.remove_prefix(lineEnd + 2);
        if (chunkSize == 0) {
            // Accept an empty trailer or bounded RFC-style trailer fields.
            if (wire == "\r\n")
                return body;
            const auto trailerEnd = wire.find("\r\n\r\n");
            if (trailerEnd == std::string_view::npos ||
                trailerEnd > kMaxHeaderSize || trailerEnd + 4 != wire.size()) {
                if (error) *error = "invalid control-key chunk trailer";
                return std::nullopt;
            }
            return body;
        }
        if (wire.size() < chunkSize + 2 ||
            wire.substr(chunkSize, 2) != "\r\n" ||
            body.size() > kMaxBodySize - chunkSize) {
            if (error) *error = "truncated or oversized control-key chunk";
            return std::nullopt;
        }
        body.append(wire.substr(0, chunkSize));
        wire.remove_prefix(chunkSize + 2);
    }
}

std::optional<std::string> decodeHttpBody(std::string_view response,
                                          std::string* error) {
    const auto headerEnd = response.find("\r\n\r\n");
    if (headerEnd == std::string_view::npos || headerEnd > kMaxHeaderSize) {
        if (error) *error = "invalid or oversized control-key HTTP header";
        return std::nullopt;
    }
    const auto firstLineEnd = response.find("\r\n");
    if (firstLineEnd == std::string_view::npos) {
        if (error) *error = "invalid control-key HTTP status";
        return std::nullopt;
    }
    const auto status = response.substr(0, firstLineEnd);
    const auto firstSpace = status.find(' ');
    if (!status.starts_with("HTTP/1.") || firstSpace == std::string_view::npos ||
        status.substr(firstSpace + 1, 3) != "200" ||
        firstSpace + 4 >= status.size() || status[firstSpace + 4] != ' ') {
        if (error) *error = "control-key endpoint returned a non-200 status";
        return std::nullopt;
    }

    std::unordered_map<std::string, std::string> headers;
    auto cursor = firstLineEnd + 2;
    while (cursor < headerEnd) {
        const auto lineEnd = response.find("\r\n", cursor);
        if (lineEnd == std::string_view::npos || lineEnd > headerEnd) {
            if (error) *error = "malformed control-key HTTP header";
            return std::nullopt;
        }
        const auto line = response.substr(cursor, lineEnd - cursor);
        const auto colon = line.find(':');
        if (colon == std::string_view::npos) {
            if (error) *error = "malformed control-key HTTP field";
            return std::nullopt;
        }
        const auto name = lower(trim(line.substr(0, colon)));
        const auto value = std::string(trim(line.substr(colon + 1)));
        if (name.empty() || headers.contains(name)) {
            if (error) *error = "duplicate control-key HTTP field";
            return std::nullopt;
        }
        headers.emplace(name, value);
        cursor = lineEnd + 2;
    }

    auto body = response.substr(headerEnd + 4);
    const auto transfer = headers.find("transfer-encoding");
    const auto length = headers.find("content-length");
    if (transfer != headers.end() && length != headers.end()) {
        if (error) *error = "ambiguous control-key HTTP framing";
        return std::nullopt;
    }
    if (transfer != headers.end()) {
        if (lower(trim(transfer->second)) != "chunked") {
            if (error) *error = "unsupported control-key transfer encoding";
            return std::nullopt;
        }
        return decodeChunked(body, error);
    }
    if (length != headers.end()) {
        std::uint64_t expected = 0;
        const auto text = trim(length->second);
        const auto result = std::from_chars(text.data(), text.data() + text.size(),
                                            expected);
        if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
            expected > kMaxBodySize || body.size() != expected) {
            if (error) *error = "invalid control-key content length";
            return std::nullopt;
        }
    } else if (body.size() > kMaxBodySize) {
        if (error) *error = "oversized control-key response body";
        return std::nullopt;
    }
    return std::string(body);
}

} // namespace

std::optional<Key32> parseControlPublicKeyResponse(std::string_view json,
                                                   std::string* error) {
    if (json.empty() || json.size() > kMaxBodySize) {
        if (error) *error = "invalid control-key response size";
        return std::nullopt;
    }
    const auto root = Json::parse(json, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        if (error) *error = "invalid control-key JSON";
        return std::nullopt;
    }
    const auto found = root.find("publicKey");
    if (found == root.end() || !found->is_string()) {
        if (error) *error = "control-key response has no TS2021 public key";
        return std::nullopt;
    }
    auto key = decodeTypedKey(found->get<std::string>(), "mkey:");
    if (!key || std::all_of(key->begin(), key->end(),
                            [](std::uint8_t byte) { return byte == 0; })) {
        if (error) *error = "control-key response contains an invalid key";
        return std::nullopt;
    }
    return key;
}

std::optional<Key32> fetchControlPublicKey(ITransport& verifiedTls,
                                           std::string_view host,
                                           std::uint16_t port,
                                           int capabilityVersion,
                                           std::string* error) {
    if (host.empty() || host.find_first_of("\r\n") != std::string_view::npos ||
        port == 0 || capabilityVersion <= 0) {
        if (error) *error = "invalid control-key request parameters";
        return std::nullopt;
    }
    if (!verifiedTls.connect(host, port, error))
        return std::nullopt;

    const auto request =
        "GET /key?v=" + std::to_string(capabilityVersion) +
        " HTTP/1.1\r\nHost: " + std::string(host) +
        "\r\nAccept: application/json\r\nConnection: close\r\n\r\n";
    const auto requestBytes = std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(request.data()), request.size());
    if (!verifiedTls.write(requestBytes, error)) {
        verifiedTls.close();
        return std::nullopt;
    }

    std::string response;
    std::array<std::uint8_t, 2048> buffer{};
    while (true) {
        const int read = verifiedTls.read(buffer.data(), buffer.size(), error);
        if (read < 0) {
            verifiedTls.close();
            return std::nullopt;
        }
        if (read == 0) break;
        if (response.size() > kMaxWireSize - static_cast<std::size_t>(read)) {
            if (error) *error = "oversized control-key HTTP response";
            verifiedTls.close();
            return std::nullopt;
        }
        response.append(reinterpret_cast<const char*>(buffer.data()),
                        static_cast<std::size_t>(read));
    }
    verifiedTls.close();
    const auto body = decodeHttpBody(response, error);
    return body ? parseControlPublicKeyResponse(*body, error) : std::nullopt;
}

} // namespace artemis::tailscale
