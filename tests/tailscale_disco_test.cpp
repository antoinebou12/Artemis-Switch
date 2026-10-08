#include "TailscaleDisco.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace artemis::tailscale;

namespace {

Key32 key(const char* hexText) {
    Key32 out{};
    for (std::size_t i = 0; i < out.size(); ++i) {
        unsigned byte = 0;
        std::sscanf(hexText + i * 2, "%2x", &byte);
        out[i] = static_cast<std::uint8_t>(byte);
    }
    return out;
}

TxId tx(std::uint8_t seed) {
    TxId id{};
    for (std::size_t i = 0; i < id.size(); ++i)
        id[i] = static_cast<std::uint8_t>(seed + i);
    return id;
}

} // namespace

int main() {
    // Same libsodium key pairs as tailscale_box_test.
    const auto aliceSk = key("000102030405060708090a0b0c0d0e0f"
                             "101112131415161718191a1b1c1d1e1f");
    const auto alicePk = key("8f40c5adb68f25624ae5b214ea767a6e"
                             "c94d829d3d7b5e1ad1ba6f3e2138285f");
    const auto bobSk = key("808182838485868788898a8b8c8d8e8f"
                           "909192939495969798999a9b9c9d9e9f");
    const auto bobPk = key("493e82fc74464a59268817623d2053c5e"
                           "b8e2cc4a988b4fee179ec6b010d531d");

    // --- endpoints ----------------------------------------------------------
    {
        const auto ep = IPv4Endpoint::parse("203.0.113.7:41641");
        assert(ep && ep->address == 0xcb007107U && ep->port == 41641);
        assert(ep->toString() == "203.0.113.7:41641");
        assert(!IPv4Endpoint::parse("203.0.113.7"));
        assert(!IPv4Endpoint::parse("203.0.113.7:0"));
        assert(!IPv4Endpoint::parse("[fd00::1]:41641"));
        assert(!IPv4Endpoint::parse("203.0.113.256:1"));
        assert(!IPv4Endpoint::parse("host.example:1"));
    }

    // --- disco plaintext layout (tailscale.com/disco) -----------------------
    {
        disco::Message ping;
        ping.type = disco::MessageType::Ping;
        ping.txid = tx(1);
        ping.nodeKey = alicePk;
        const auto plain = disco::encodePlaintext(ping);
        assert(plain.size() == 2 + 12 + 32);
        assert(plain[0] == 0x01 && plain[1] == 0x00 && plain[2] == 1);

        disco::Message pong;
        pong.type = disco::MessageType::Pong;
        pong.txid = tx(9);
        pong.source = *IPv4Endpoint::parse("198.51.100.4:3478");
        const auto pongPlain = disco::encodePlaintext(pong);
        assert(pongPlain.size() == 2 + 12 + 18);
        // ::ffff:198.51.100.4, port 3478 big-endian
        assert(pongPlain[14 + 10] == 0xff && pongPlain[14 + 11] == 0xff);
        assert(pongPlain[14 + 12] == 198 && pongPlain[14 + 15] == 4);
        assert(pongPlain[14 + 16] == 0x0d && pongPlain[14 + 17] == 0x96);
        const auto decoded = disco::decodePlaintext(pongPlain);
        assert(decoded && decoded->type == disco::MessageType::Pong);
        assert(decoded->txid == tx(9) && decoded->source == pong.source);

        disco::Message cmm;
        cmm.type = disco::MessageType::CallMeMaybe;
        cmm.endpoints = {*IPv4Endpoint::parse("192.168.1.20:41641"),
                         *IPv4Endpoint::parse("203.0.113.7:12345")};
        const auto cmmDecoded =
            disco::decodePlaintext(disco::encodePlaintext(cmm));
        assert(cmmDecoded && cmmDecoded->endpoints == cmm.endpoints);

        // Truncated and unknown messages are rejected, never misparsed.
        assert(!disco::decodePlaintext(std::vector<std::uint8_t>{0x02, 0, 1}));
        assert(!disco::decodePlaintext(std::vector<std::uint8_t>{0x7f, 0}));
        assert(!disco::decodePlaintext(
            std::vector<std::uint8_t>{0x03, 0, 1, 2, 3}));
        // An old-style ping without a node key is still a ping.
        std::vector<std::uint8_t> shortPing{0x01, 0};
        shortPing.insert(shortPing.end(), 12, 0xaa);
        const auto oldPing = disco::decodePlaintext(shortPing);
        assert(oldPing && !oldPing->hasNodeKey);
    }

    // --- sealed packets round-trip and authenticate ------------------------
    {
        disco::Message ping;
        ping.type = disco::MessageType::Ping;
        ping.txid = tx(3);
        ping.nodeKey = alicePk;
        std::array<std::uint8_t, disco::kNonceLen> nonce{};
        nonce[0] = 7;
        const auto packet = disco::seal(ping, aliceSk, alicePk, bobPk, nonce);
        assert(packet.size() == disco::kHeaderLen + 16 + 2 + 12 + 32);
        assert(disco::looksLikeDisco(packet));
        assert(*disco::senderKey(packet) == alicePk);

        const auto opened = disco::open(packet, bobSk);
        assert(opened && opened->sender == alicePk);
        assert(opened->message.type == disco::MessageType::Ping);
        assert(opened->message.txid == tx(3));
        assert(opened->message.hasNodeKey && opened->message.nodeKey == alicePk);

        // Wrong recipient, tampered body, forged sender: all rejected.
        assert(!disco::open(packet, aliceSk));
        auto tampered = packet;
        tampered.back() ^= 1;
        assert(!disco::open(tampered, bobSk));
        auto forged = packet;
        forged[6] ^= 1;
        assert(!disco::open(forged, bobSk));
        // WireGuard packets are never mistaken for disco.
        const std::vector<std::uint8_t> wg(148, 0x01);
        assert(!disco::looksLikeDisco(wg));
    }

    // --- STUN ----------------------------------------------------------------
    {
        const auto txid = tx(0x40);
        // CRC-32 check value for "123456789" is 0xCBF43926.
        const std::string check = "123456789";
        assert(stun::fingerprint(std::span<const std::uint8_t>(
                   reinterpret_cast<const std::uint8_t*>(check.data()),
                   check.size())) == (0xCBF43926U ^ 0x5354554eU));

        // Tailscale's STUN server only answers requests carrying SOFTWARE
        // "tailnode" followed by a valid FINGERPRINT (net/stun.Request).
        const auto request = stun::bindingRequest(txid);
        assert(request.size() == 20 + 12 + 8);
        assert(request[0] == 0x00 && request[1] == 0x01);
        assert(request[2] == 0x00 && request[3] == 20); // attribute bytes
        assert(request[4] == 0x21 && request[5] == 0x12 && request[6] == 0xa4 &&
               request[7] == 0x42);
        assert(std::equal(txid.begin(), txid.end(), request.begin() + 8));
        assert(request[20] == 0x80 && request[21] == 0x22 && request[22] == 0 &&
               request[23] == 8);
        assert(std::string(request.begin() + 24, request.begin() + 32) ==
               "tailnode");
        assert(request[32] == 0x80 && request[33] == 0x28 && request[34] == 0 &&
               request[35] == 4);
        const std::uint32_t fp = (std::uint32_t{request[36]} << 24U) |
                                 (std::uint32_t{request[37]} << 16U) |
                                 (std::uint32_t{request[38]} << 8U) | request[39];
        assert(fp == stun::fingerprint(std::span<const std::uint8_t>(
                         request.data(), 32)));
        assert(stun::looksLikeStun(request));

        // Binding success with XOR-MAPPED-ADDRESS 203.0.113.7:41641.
        std::vector<std::uint8_t> response{0x01, 0x01, 0x00, 0x0c,
                                           0x21, 0x12, 0xa4, 0x42};
        response.insert(response.end(), txid.begin(), txid.end());
        const std::uint16_t xport = 41641 ^ 0x2112;
        const std::uint32_t xaddr = 0xcb007107U ^ 0x2112A442U;
        const std::vector<std::uint8_t> attr{
            0x00, 0x20, 0x00, 0x08, 0x00, 0x01,
            static_cast<std::uint8_t>(xport >> 8),
            static_cast<std::uint8_t>(xport & 0xff),
            static_cast<std::uint8_t>(xaddr >> 24),
            static_cast<std::uint8_t>(xaddr >> 16),
            static_cast<std::uint8_t>(xaddr >> 8),
            static_cast<std::uint8_t>(xaddr)};
        response.insert(response.end(), attr.begin(), attr.end());
        const auto mapped = stun::parseBindingResponse(response, txid);
        assert(mapped && mapped->toString() == "203.0.113.7:41641");
        // Another transaction's response is ignored.
        assert(!stun::parseBindingResponse(response, tx(0x41)));
        // A length field running past the packet is rejected.
        auto lying = response;
        lying[3] = 0x40;
        assert(!stun::parseBindingResponse(lying, txid));
        // Disco and WireGuard packets are not STUN.
        assert(!stun::looksLikeStun(std::vector<std::uint8_t>(32, 0x04)));
    }

    // --- direct path selection --------------------------------------------
    {
        using Clock = DirectPath::Clock;
        const auto t0 = Clock::time_point{} + std::chrono::hours(1);
        const auto lan = *IPv4Endpoint::parse("192.168.1.20:41641");
        const auto wan = *IPv4Endpoint::parse("203.0.113.7:41641");
        DirectPath path;
        path.addCandidate(lan);
        path.addCandidate(wan);
        path.addCandidate(lan); // duplicate ignored
        assert(path.candidateCount() == 2);
        assert(!path.best(t0));

        auto due = path.due(t0);
        assert(due.size() == 2);
        path.notePingSent(tx(1), lan, t0);
        path.notePingSent(tx(2), wan, t0);
        assert(path.due(t0 + std::chrono::seconds(1)).empty());

        // A pong from the wrong address or with an unknown txid is ignored.
        assert(!path.notePong(tx(1), wan, t0 + std::chrono::milliseconds(5)));
        assert(!path.notePong(tx(9), lan, t0 + std::chrono::milliseconds(5)));
        assert(!path.best(t0));

        path.notePingSent(tx(1), lan, t0);
        const auto wanRtt =
            path.notePong(tx(2), wan, t0 + std::chrono::milliseconds(40));
        assert(wanRtt && *wanRtt == 40);
        assert(path.best(t0 + std::chrono::milliseconds(40)) == wan);
        // The LAN path is clearly faster and takes over.
        const auto lanRtt =
            path.notePong(tx(1), lan, t0 + std::chrono::milliseconds(2));
        assert(lanRtt && *lanRtt == 2);
        assert(path.best(t0 + std::chrono::milliseconds(50)) == lan);
        assert(path.bestRttMs() == 2);

        // Without fresh pongs the path expires and traffic returns to DERP.
        assert(path.best(t0 + std::chrono::seconds(6)));
        assert(!path.best(t0 + std::chrono::seconds(7)));
        // Heartbeat is due again after 3 s.
        assert(path.due(t0 + std::chrono::seconds(3)).size() == 2);

        path.reset();
        assert(path.candidateCount() == 0 && !path.best(t0));
    }
    return 0;
}
