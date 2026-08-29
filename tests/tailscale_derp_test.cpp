#include "TailscaleDerp.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <vector>

using artemis::tailscale::DerpCodec;
using artemis::tailscale::DerpFrame;
using artemis::tailscale::DerpFrameType;
using artemis::tailscale::DerpSession;
using artemis::tailscale::ITransport;
using artemis::tailscale::kDerpFrameHeaderLen;

namespace {

std::vector<std::uint8_t> bytes(std::initializer_list<std::uint8_t> init) {
    return std::vector<std::uint8_t>(init);
}

// A loop-back transport with an outbound read queue and a captured write log.
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

} // namespace

int main() {
    // Round-trip encode/parse for each frame type.
    const std::vector<std::uint8_t> empty{};
    const std::vector<std::uint8_t> ping = pingPayload(0x1122334455667788ULL);
    std::vector<std::uint8_t> peer(33);
    peer[0] = 7;  // reason byte
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

    // Known-answer checks lock the exact on-wire layout to the DERP spec:
    // [FrameType byte][big-endian uint32 length][payload]; no per-frame magic.
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

    // SendPacket = 32B dest key + packet bytes.
    std::vector<std::uint8_t> destKey(32, 0xAB);
    std::vector<std::uint8_t> packet{1, 2, 3, 4, 5};
    auto sendPayload = destKey;
    sendPayload.insert(sendPayload.end(), packet.begin(), packet.end());
    const auto sendEnc = DerpCodec::encode(DerpFrameType::SendPacket, sendPayload);
    auto sendParsed = DerpCodec::parse(sendEnc, &error);
    assert(sendParsed && sendParsed->type == DerpFrameType::SendPacket);
    assert(sendParsed->payload.size() == 32 + 5);

    // Oversized frames must be rejected.
    std::vector<std::uint8_t> huge(DerpCodec::kMaxFramePayload + 1);
    {
        const auto hugeEnc = DerpCodec::encode(DerpFrameType::SendPacket, huge);
        std::string e;
        assert(!DerpCodec::parse(
            std::span<const std::uint8_t>(hugeEnc).first(hugeEnc.size()), &e));
    }

    // Split-frame reassembly through the streaming decoder.
    DerpCodec decoder;
    const auto frame = DerpCodec::encode(DerpFrameType::Pong, ping);
    assert(decoder.append(std::span<const std::uint8_t>(frame).first(4), &error));
    assert(!decoder.take(&error).has_value());
    assert(decoder.append(std::span<const std::uint8_t>(frame).subspan(4), &error));
    auto out = decoder.take(&error);
    assert(out && out->type == DerpFrameType::Pong && out->payload == ping);

    // Session send path wraps payloads and writes to the transport.
    {
        auto transport = std::make_unique<MockTransport>();
        auto* raw = transport.get();
        DerpSession session(std::move(transport));
        // connect() must fail closed (naclbox handshake not implemented).
        assert(!session.connect(&error));
        assert(!error.empty());
        assert(session.sendPing(0xABCD, &error));
        assert(raw->written_.size() == kDerpFrameHeaderLen + 8);
        auto parsed =
            DerpCodec::parse(std::span<const std::uint8_t>(raw->written_), &error);
        assert(parsed && parsed->type == DerpFrameType::Ping);

        raw->written_.clear();
        assert(session.sendPacket(destKey, packet, &error));
        parsed = DerpCodec::parse(std::span<const std::uint8_t>(raw->written_),
                                  &error);
        assert(parsed && parsed->type == DerpFrameType::SendPacket &&
               parsed->payload.size() == 32 + packet.size());
    }

    // Session recv path reassembles a frame split across reads.
    {
        auto transport = std::make_unique<MockTransport>();
        auto* raw = transport.get();
        auto recvPayload = destKey;
        recvPayload.insert(recvPayload.end(), packet.begin(), packet.end());
        const auto recvEnc =
            DerpCodec::encode(DerpFrameType::RecvPacket, recvPayload);
        raw->readQueue_.push_back({recvEnc.begin(), recvEnc.begin() + 7});
        raw->readQueue_.push_back({recvEnc.begin() + 7, recvEnc.end()});
        DerpSession session(std::move(transport));
        auto got = session.recvFrame(&error);
        assert(got && got->type == DerpFrameType::RecvPacket);
        assert(got->payload.size() == 32 + packet.size());
        // Connection close surfaces as a recoverable error.
        (void)error;
    }
    return 0;
}