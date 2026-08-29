#include "../app/src/remote_access/tailscale/TailscaleControlKey.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

using namespace artemis::tailscale;

namespace {

class FakeTransport final : public ITransport {
public:
    std::string response;
    std::string request;
    bool connected = false;
    std::size_t cursor = 0;

    bool connect(std::string_view host, std::uint16_t port,
                 std::string*) override {
        connected = host == "controlplane.tailscale.com" && port == 443;
        return connected;
    }
    int read(std::uint8_t* buffer, std::size_t length, std::string*) override {
        if (cursor == response.size()) return 0;
        const auto count = std::min(length, response.size() - cursor);
        std::memcpy(buffer, response.data() + cursor, count);
        cursor += count;
        return static_cast<int>(count);
    }
    bool write(std::span<const std::uint8_t> bytes, std::string*) override {
        request.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        return true;
    }
    void close() noexcept override { connected = false; }
};

constexpr const char* kPublicKey =
    "mkey:0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20";

} // namespace

int main() {
    std::string error;
    const std::string json = std::string("{\"publicKey\":\"") + kPublicKey +
                             "\",\"legacyPublicKey\":\"\"}";
    auto key = parseControlPublicKeyResponse(json, &error);
    assert(key && key->front() == 1 && key->back() == 32);
    assert(!parseControlPublicKeyResponse(
        "{\"publicKey\":\"mkey:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"}",
        &error));
    assert(!parseControlPublicKeyResponse("{\"legacyPublicKey\":\"x\"}",
                                          &error));

    FakeTransport fixed;
    fixed.response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                     "Content-Length: " + std::to_string(json.size()) +
                     "\r\n\r\n" + json;
    key = fetchControlPublicKey(fixed, "controlplane.tailscale.com", 443, 68,
                                &error);
    assert(key && key->front() == 1 && key->back() == 32);
    assert(fixed.request.find("GET /key?v=68 HTTP/1.1\r\n") == 0);
    assert(fixed.request.find("Connection: close\r\n") != std::string::npos);

    FakeTransport chunked;
    chunked.response =
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n" +
        [] (const std::string& body) {
            char size[32];
            std::snprintf(size, sizeof(size), "%zx", body.size());
            return std::string(size) + "\r\n" + body + "\r\n0\r\n\r\n";
        }(json);
    assert(fetchControlPublicKey(chunked, "controlplane.tailscale.com", 443,
                                 68, &error));

    FakeTransport ambiguous;
    ambiguous.response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n"
                         "Transfer-Encoding: chunked\r\n\r\n0\r\n\r\n";
    assert(!fetchControlPublicKey(ambiguous, "controlplane.tailscale.com", 443,
                                  68, &error));
    assert(error == "ambiguous control-key HTTP framing");

    FakeTransport rejected;
    rejected.response = "HTTP/1.1 500 Error\r\nContent-Length: 0\r\n\r\n";
    assert(!fetchControlPublicKey(rejected, "controlplane.tailscale.com", 443,
                                  68, &error));
    return 0;
}
