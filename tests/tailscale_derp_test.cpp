#include "TailscaleDerp.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <vector>

using artemis::tailscale::DerpCodec;
using artemis::tailscale::DerpCrypto;
using artemis::tailscale::DerpFrame;
using artemis::tailscale::DerpFrameType;
using artemis::tailscale::DerpSession;
using artemis::tailscale::ITransport;
using artemis::tailscale::Key32;
using artemis::tailscale::kDerpFrameHeaderLen;
using artemis::tailscale::kDerpKeyLen;
using artemis::tailscale::kDerpMagic;
using artemis::tailscale::kDerpNonceLen;

namespace {

class MockTransport final : public ITransport {
public:
    bool connect(std::string_view, std::uint16_t, std::string*) override {
        return true;
    }
    int read(std::uint8_t* buffer, std::size_t length,
             std::string* error) override {
        (void)error;
        if (readQueue_.empty()) return 0;
        auto& front = readQueue_.front();
        const auto n = std::min(length, front.size());
        std::copy_n(front.begin(), n, buffer);
        front.erase(front.begin(), front.begin() + n);
        if (front.empty()) readQueue_.pop_front();
        return static_cast<int>(n);
    }
    bool write(std::span<const std::uint8_t> data, std::string*) override {
        written_.insert(written_.end(), data.begin(), data.end());
        return true;
    }
    void close() noexcept override {}
    std::deque<std::vector<std::uint8_t>> readQueue_;
    std::vector<std::uint8_t> written_;
};

std::vector<std::uint8_t> pingPayload(std::uint64_t token) {
    std::vector<std::uint8_t> out(8);
    for (std::size_t i = 0; i < 8; ++i)
        out[i] = static_cast<std::uint8_t>(token >> (i * 8));
    return out;
}

extern "C" {
#include <monocypher.h>
}

Key32 makePrivateKey(std::uint8_t seed) {
    Key32 key{};
    for (std::size_t i = 0; i < key.size(); ++i)
        key[i] = static_cast<std::uint8_t>(seed + i * 3);
    return key;
}

Key32 derivePublic(const Key32& priv) {
    Key32 pub{};
    crypto_x25519_public_key(pub.data(), priv.data());
    return pub;
}

} // namespace

int main() {
    // 1. Round-trip encode/parse for each frame type.
    const std::vector<std::uint8_t> empty{};
    const std::vector<std::uint8_t> ping = pingPayload(0x1122334455667788ULL);
    std::vector<std::uint8_t> peer(33);
    peer[0] = 7;
    std::string error;

    for (const auto& entry : {std::pair{DerpFrameType::KeepAlive, empty},
                              {DerpFrameType::Ping, ping},
                              {DerpFrameType::Pong, ping},
                              {DerpFrameType::PeerGone, peer}}) {
        const auto encoded = DerpCodec::encode(entry.first, entry.second);
        auto parsed = DerpCodec::parse(encoded, &error);
        assert(parsed.has_value());
        assert(parsed->type == entry.first);
        assert(parsed->payload == entry.second);
    }

    // 2. Known-answer layout checks.
    const auto pingKA = DerpCodec::encode(DerpFrameType::Ping, ping);
    assert(pingKA.size() == 5 + 8);
    {
        const std::vector<std::uint8_t> header(pingKA.begin(), pingKA.begin() + 5);
        assert((header == std::vector<std::uint8_t>{0x12, 0x00, 0x00, 0x00, 0x08}));
    }
    const auto keepAliveKA = DerpCodec::encode(DerpFrameType::KeepAlive, {});
    assert(keepAliveKA.size() == 5);
    {
        const std::vector<std::uint8_t> header(keepAliveKA.begin(),
                                               keepAliveKA.begin() + 5);
        assert((header == std::vector<std::uint8_t>{0x06, 0x00, 0x00, 0x00, 0x00}));
    }

    // 3. SendPacket = 32B dest key + packet bytes.
    std::vector<std::uint8_t> destKey(32, 0xAB);
    std::vector<std::uint8_t> packet{1, 2, 3, 4, 5};
    auto sendPayload = destKey;
    sendPayload.insert(sendPayload.end(), packet.begin(), packet.end());
    const auto sendEnc = DerpCodec::encode(DerpFrameType::SendPacket, sendPayload);
    auto sendParsed = DerpCodec::parse(sendEnc, &error);
    assert(sendParsed && sendParsed->type == DerpFrameType::SendPacket);
    assert(sendParsed->payload.size() == 32 + 5);

    // 4. Split-frame reassembly through streaming decoder.
    DerpCodec decoder;
    const auto frame = DerpCodec::encode(DerpFrameType::Pong, ping);
    assert(decoder.append(std::span<const std::uint8_t>(frame).first(4), &error));
    assert(!decoder.take(&error).has_value());
    assert(decoder.append(std::span<const std::uint8_t>(frame).subspan(4), &error));
    auto out = decoder.take(&error);
    assert(out && out->type == DerpFrameType::Pong && out->payload == ping);

    // 5. Unconfigured session connect fails closed.
    {
        auto transport = std::make_unique<MockTransport>();
        auto* raw = transport.get();
        DerpSession session(std::move(transport));
        assert(!session.connect(&error));
        assert(!error.empty());
        assert(!session.isConnected());

        // Raw send works on transport
        assert(session.sendPing(0xABCD, &error));
        assert(raw->written_.size() == kDerpFrameHeaderLen + 8);
        auto parsed =
            DerpCodec::parse(std::span<const std::uint8_t>(raw->written_), &error);
        assert(parsed && parsed->type == DerpFrameType::Ping);
    }

    // 6. DerpCrypto seal and open round-trip.
    {
        DerpCrypto crypto;
        const auto alicePrivate = makePrivateKey(1);
        const auto alicePublic = derivePublic(alicePrivate);
        const auto bobPrivate = makePrivateKey(3);
        const auto bobPublic = derivePublic(bobPrivate);

        std::array<std::uint8_t, kDerpNonceLen> nonce{};
        for (std::size_t i = 0; i < nonce.size(); ++i)
            nonce[i] = static_cast<std::uint8_t>(i + 1);

        const std::string message = "hello derp relay";
        std::vector<std::uint8_t> plainText(message.begin(), message.end());
        std::vector<std::uint8_t> cipherText;

        // Alice seals for Bob
        assert(crypto.seal(plainText, alicePrivate, bobPublic, nonce, cipherText, &error));
        assert(cipherText.size() == plainText.size() + 16);

        // Bob opens from Alice
        std::vector<std::uint8_t> decrypted;
        assert(crypto.open(cipherText, bobPrivate, alicePublic, nonce, decrypted, &error));
        assert(decrypted == plainText);

        // Tampered ciphertext fails
        cipherText[0] ^= 0xFF;
        assert(!crypto.open(cipherText, bobPrivate, alicePublic, nonce, decrypted, &error));
    }

    // 7. Full authenticated handshake between client and mock DERP server.
    {
        auto transport = std::make_unique<MockTransport>();
        auto* raw = transport.get();

        const auto clientPrivate = makePrivateKey(11);
        const auto clientPublic = derivePublic(clientPrivate);
        const auto serverPrivate = makePrivateKey(21);
        const auto serverPublic = derivePublic(serverPrivate);

        // Prepare ServerKey greeting in server's outbound queue
        std::vector<std::uint8_t> serverKeyPayload(kDerpMagic.begin(), kDerpMagic.end());
        serverKeyPayload.insert(serverKeyPayload.end(), serverPublic.begin(), serverPublic.end());
        const auto serverKeyFrame = DerpCodec::encode(DerpFrameType::ServerKey, serverKeyPayload);
        raw->readQueue_.push_back(serverKeyFrame);

        // Also prepare ServerInfo response in queue (will be read after ClientInfo is sent)
        DerpCrypto crypto;
        std::array<std::uint8_t, kDerpNonceLen> srvNonce{};
        srvNonce.fill(0x77);
        const std::string srvJson = R"({"canRelay":true})";
        std::vector<std::uint8_t> sealedSrvInfo;
        assert(crypto.seal(
            std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(srvJson.data()), srvJson.size()),
            serverPrivate, clientPublic, srvNonce, sealedSrvInfo, &error));

        std::vector<std::uint8_t> srvPayload(srvNonce.begin(), srvNonce.end());
        srvPayload.insert(srvPayload.end(), sealedSrvInfo.begin(), sealedSrvInfo.end());
        const auto serverInfoFrame = DerpCodec::encode(DerpFrameType::ServerInfo, srvPayload);
        raw->readQueue_.push_back(serverInfoFrame);

        error.clear();
        DerpSession session(std::move(transport), clientPrivate, clientPublic);
        assert(session.connect(&error));
        assert(error.empty());
        assert(session.isConnected());
        assert(session.serverKey() == serverPublic);

        // Handshake verified: Client sent ClientInfo frame
        assert(!raw->written_.empty());
        auto clientFrame = DerpCodec::parse(raw->written_, &error);
        assert(clientFrame && clientFrame->type == DerpFrameType::ClientInfo);
        assert(clientFrame->payload.size() >= kDerpKeyLen + kDerpNonceLen);

        // Post-handshake: sending packet over authenticated DERP session
        raw->written_.clear();
        assert(session.sendPacket(destKey, packet, &error));
        auto packetFrame = DerpCodec::parse(raw->written_, &error);
        assert(packetFrame && packetFrame->type == DerpFrameType::SendPacket);

        session.close();
        assert(!session.isConnected());
    }

    // 8. DERP HTTP upgrade: request shape and response validation.
    {
        const auto request = artemis::tailscale::buildDerpUpgradeRequest("derp1.example");
        assert(request.starts_with("GET /derp HTTP/1.1\r\n"));
        assert(request.find("Host: derp1.example\r\n") != std::string::npos);
        assert(request.find("Upgrade: DERP\r\n") != std::string::npos);
        assert(request.ends_with("\r\n\r\n"));

        std::string error;
        assert(artemis::tailscale::validateDerpUpgradeResponse(
            "HTTP/1.1 101 Switching Protocols\r\nUpgrade: DERP\r\n"
            "Connection: Upgrade\r\n\r\n",
            &error));
        // Header-name case and extra upgrade tokens are tolerated.
        assert(artemis::tailscale::validateDerpUpgradeResponse(
            "HTTP/1.1 101 Switching Protocols\r\nupgrade: websocket, Derp\r\n\r\n",
            &error));
        assert(!artemis::tailscale::validateDerpUpgradeResponse(
            "HTTP/1.1 200 OK\r\nUpgrade: DERP\r\n\r\n", &error));
        assert(!artemis::tailscale::validateDerpUpgradeResponse(
            "HTTP/1.1 101 Switching Protocols\r\nConnection: Upgrade\r\n\r\n",
            &error));
        assert(!artemis::tailscale::validateDerpUpgradeResponse(
            "HTTP/1.1 101 Switching Protocols\r\nUpgrade: DERP\r\n", &error));
    }

    return 0;
}
