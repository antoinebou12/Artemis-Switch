// These tests report failures through assert(). Keep them live in every
// build type (Release defines NDEBUG), including asserts with side effects.
#undef NDEBUG

#include "TailscaleHttp2.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace artemis::tailscale;

namespace {

using Bytes = std::vector<std::uint8_t>;

Bytes bytesOf(std::string_view text) {
    return Bytes(text.begin(), text.end());
}

std::uint32_t be32(const Bytes& bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U) |
           static_cast<std::uint32_t>(bytes[offset + 3]);
}

Bytes concat(const Bytes& a, const Bytes& b) {
    Bytes out = a;
    out.insert(out.end(), b.begin(), b.end());
    return out;
}

// Pulls every complete frame out of `decoder`.
std::vector<Http2Frame> drain(Http2FrameDecoder& decoder) {
    std::vector<Http2Frame> frames;
    while (auto frame = decoder.take())
        frames.push_back(std::move(*frame));
    return frames;
}

void windowUpdateMasksReservedBit() {
    // RFC 9113 4.1 / 6.9: the high bit of the stream id and of the increment
    // is reserved and must be sent as zero.
    const Bytes frame = buildHttp2WindowUpdate(0x80000003U, 0x80000007U);
    const Bytes expected = {0x00, 0x00, 0x04, 0x08, 0x00,
                            0x00, 0x00, 0x00, 0x03, // stream 3
                            0x00, 0x00, 0x00, 0x07}; // increment 7
    assert(frame == expected);
}

void windowUpdateTargetsConnectionWithStreamZero() {
    const Bytes frame = buildHttp2WindowUpdate(0, 1000);
    assert(frame.size() == 13);
    assert(frame[3] == 0x08);
    assert(be32(frame, 5) == 0);
    assert(be32(frame, 9) == 1000);
}

void settingsAckIsExactlyTheRfcFrame() {
    // Empty SETTINGS frame with the ACK flag on stream 0.
    const Bytes expected = {0x00, 0x00, 0x00, 0x04, 0x01,
                            0x00, 0x00, 0x00, 0x00};
    assert(buildHttp2SettingsAck() == expected);
}

void pingAckEchoesOpaqueDataWithAckFlag() {
    const std::array<std::uint8_t, 8> opaque = {1, 2, 3, 4, 5, 6, 7, 8};
    const Bytes frame = buildHttp2PingAck(opaque);
    const Bytes expected = {0x00, 0x00, 0x08, 0x06, 0x01,
                            0x00, 0x00, 0x00, 0x00,
                            1,    2,    3,    4,    5,    6,    7,    8};
    assert(frame == expected);

    // A non-standard length is passed through rather than truncated or padded.
    const Bytes odd = buildHttp2PingAck(std::array<std::uint8_t, 3>{9, 8, 7});
    assert(odd.size() == 9 + 3);
    assert(odd[2] == 3);
}

void clientPrefaceLayout() {
    const Bytes preface = buildHttp2ClientPreface();
    // 24-byte magic + SETTINGS (9 + 6) + connection WINDOW_UPDATE (9 + 4).
    assert(preface.size() == 24 + 15 + 13);
    assert(std::string(preface.begin(), preface.begin() + 24) ==
           "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n");

    Http2FrameDecoder decoder;
    assert(decoder.append(std::span<const std::uint8_t>(preface).subspan(24)));
    const auto frames = drain(decoder);
    assert(frames.size() == 2);

    // SETTINGS { INITIAL_WINDOW_SIZE (0x4) = stream window }.
    const Http2Frame& settings = frames[0];
    assert(settings.type == 0x04);
    assert(settings.flags == 0x00);
    assert(settings.streamId == 0);
    assert(settings.length == 6);
    assert(settings.payload[0] == 0x00 && settings.payload[1] == 0x04);
    assert(be32(settings.payload, 2) == kHttp2ClientStreamWindow);

    // The connection window is not part of SETTINGS, so it is raised with a
    // WINDOW_UPDATE on stream 0 up to the advertised connection window.
    const Http2Frame& update = frames[1];
    assert(update.type == 0x08);
    assert(update.streamId == 0);
    assert(update.length == 4);
    assert(be32(update.payload, 0) ==
           kHttp2ClientConnectionWindow - kHttp2DefaultWindow);
}

void advertisedWindowsAreOrderedAndLegal() {
    // The connection window must be able to hold at least one full stream
    // window, and neither may exceed the 2^31-1 protocol maximum.
    assert(kHttp2DefaultWindow == 65535);
    assert(kHttp2ClientStreamWindow > kHttp2DefaultWindow);
    assert(kHttp2ClientConnectionWindow >= kHttp2ClientStreamWindow);
    assert(kHttp2ClientConnectionWindow <= 0x7fffffffU);
}

void postHeadersMatchHpackWireFormat() {
    constexpr std::string_view authority = "controlplane.tailscale.com";
    constexpr std::string_view path = "/machine/map";
    const Bytes frame = buildHttp2PostHeaders(1, authority, path);

    // Expected HPACK (RFC 7541), written out independently of the encoder:
    //   0x83                 indexed :method POST
    //   0x87                 indexed :scheme https
    //   0x04 len path        literal, name index 4  (:path)
    //   0x01 len authority   literal, name index 1  (:authority)
    //   0x0f 0x10 len value  literal, name index 15+16=31 (content-type)
    Bytes hpack = {0x83, 0x87, 0x04, static_cast<std::uint8_t>(path.size())};
    hpack = concat(hpack, bytesOf(path));
    hpack.push_back(0x01);
    hpack.push_back(static_cast<std::uint8_t>(authority.size()));
    hpack = concat(hpack, bytesOf(authority));
    hpack.push_back(0x0f);
    hpack.push_back(0x10);
    hpack.push_back(16);
    hpack = concat(hpack, bytesOf("application/json"));

    assert(frame.size() == 9 + hpack.size());
    assert(frame[0] == 0x00 && frame[1] == 0x00 &&
           frame[2] == static_cast<std::uint8_t>(hpack.size()));
    assert(frame[3] == 0x01); // HEADERS
    assert(frame[4] == 0x04); // END_HEADERS only: the request body follows
    assert(be32(frame, 5) == 1);
    assert(Bytes(frame.begin() + 9, frame.end()) == hpack);
}

void postHeadersUseTheGivenStream() {
    const Bytes frame = buildHttp2PostHeaders(0x7fffffffU, "a", "/b");
    assert(be32(frame, 5) == 0x7fffffffU);
}

void dataFrameFlagsAndPayload() {
    const Bytes body = bytesOf("{\"Version\":133}");

    const Bytes open = buildHttp2DataFrame(3, body);
    assert(open[3] == 0x00);
    assert(open[4] == 0x00); // END_STREAM clear by default
    assert(be32(open, 5) == 3);
    assert(Bytes(open.begin() + 9, open.end()) == body);

    const Bytes last = buildHttp2DataFrame(3, body, true);
    assert(last[4] == 0x01); // END_STREAM

    // An empty END_STREAM DATA frame is the idiomatic way to half-close.
    const Bytes empty = buildHttp2DataFrame(5, {}, true);
    const Bytes expected = {0x00, 0x00, 0x00, 0x00, 0x01,
                            0x00, 0x00, 0x00, 0x05};
    assert(empty == expected);
}

void builtFramesRoundTripThroughDecoder() {
    const Bytes body = bytesOf("hello control plane");
    Bytes wire = buildHttp2SettingsAck();
    wire = concat(wire, buildHttp2DataFrame(7, body, true));
    wire = concat(wire, buildHttp2PingAck(std::array<std::uint8_t, 8>{}));

    Http2FrameDecoder decoder;
    assert(decoder.append(wire));
    const auto frames = drain(decoder);
    assert(frames.size() == 3);

    assert(frames[0].type == 0x04 && frames[0].flags == 0x01 &&
           frames[0].length == 0 && frames[0].payload.empty());
    assert(frames[1].type == 0x00 && frames[1].flags == 0x01 &&
           frames[1].streamId == 7 && frames[1].payload == body &&
           frames[1].length == body.size());
    assert(frames[2].type == 0x06 && frames[2].flags == 0x01 &&
           frames[2].payload.size() == 8);

    // Nothing is left over.
    assert(!decoder.take());
}

void decoderWaitsForCompleteFramesWhenFedOneByteAtATime() {
    const Bytes body = bytesOf("split across many reads");
    const Bytes wire = buildHttp2DataFrame(9, body);

    Http2FrameDecoder decoder;
    for (std::size_t i = 0; i + 1 < wire.size(); ++i) {
        assert(decoder.append(std::span<const std::uint8_t>(&wire[i], 1)));
        assert(!decoder.take()); // header or payload still incomplete
    }
    assert(decoder.append(std::span<const std::uint8_t>(&wire.back(), 1)));
    const auto frame = decoder.take();
    assert(frame.has_value());
    assert(frame->streamId == 9 && frame->payload == body);
    assert(!decoder.take());
}

void decoderKeepsTrailingPartialFrame() {
    const Bytes first = buildHttp2DataFrame(1, bytesOf("one"));
    const Bytes second = buildHttp2DataFrame(1, bytesOf("two"));

    Bytes wire = first;
    wire.insert(wire.end(), second.begin(), second.begin() + 5);

    Http2FrameDecoder decoder;
    assert(decoder.append(wire));
    const auto a = decoder.take();
    assert(a.has_value() && a->payload == bytesOf("one"));
    assert(!decoder.take()); // second frame is only partially buffered

    assert(decoder.append(std::span<const std::uint8_t>(second).subspan(5)));
    const auto b = decoder.take();
    assert(b.has_value() && b->payload == bytesOf("two"));
}

void decoderMasksReservedStreamIdBit() {
    Bytes wire = buildHttp2DataFrame(0, {});
    wire[5] = 0x80; // R bit set on the wire
    wire[8] = 0x05;
    Http2FrameDecoder decoder;
    assert(decoder.append(wire));
    const auto frame = decoder.take();
    assert(frame.has_value());
    assert(frame->streamId == 5);
}

void decoderAcceptsMaximumSizedFrameAndRejectsOverflow() {
    constexpr std::size_t kMax = Http2FrameDecoder::kMaxFrameSize;
    Bytes big(kMax, 0xab);
    const Bytes wire = buildHttp2DataFrame(1, big);
    assert(wire.size() == kMax + 9);

    Http2FrameDecoder decoder;
    assert(decoder.append(wire));
    // The buffer is now exactly full; one more byte must be refused.
    assert(!decoder.append(std::array<std::uint8_t, 1>{0}));
    const auto frame = decoder.take();
    assert(frame.has_value());
    assert(frame->length == kMax && frame->payload.size() == kMax);

    // A refused append leaves the decoder usable afterwards.
    assert(decoder.append(buildHttp2SettingsAck()));
    assert(decoder.take().has_value());
}

void decoderDropsOversizedFrameAndRecovers() {
    // Declares a payload one byte over the limit (0x100001).
    const Bytes poisoned = {0x10, 0x00, 0x01, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x01};
    Http2FrameDecoder decoder;
    assert(decoder.append(poisoned));
    assert(!decoder.take());

    // The poisoned header was discarded, so a following valid frame decodes
    // cleanly instead of being parsed behind stale bytes.
    assert(decoder.append(buildHttp2SettingsAck()));
    const auto frame = decoder.take();
    assert(frame.has_value());
    assert(frame->type == 0x04 && frame->flags == 0x01);
}

void receiveWindowWaitsForHalfBeforeReplenishing() {
    Http2ReceiveWindow window(100);
    assert(!window.consume(0));
    assert(!window.consume(49)); // 49 < 50: still under half
    const auto increment = window.consume(1); // 50 reaches half
    assert(increment.has_value());
    assert(*increment == 50);

    // The credit was handed back, so accounting starts over.
    assert(!window.consume(10));
    const auto second = window.consume(40);
    assert(second.has_value() && *second == 50);
}

void receiveWindowReturnsEverythingConsumedInOneUpdate() {
    Http2ReceiveWindow window(100);
    const auto increment = window.consume(90); // one large DATA frame
    assert(increment.has_value() && *increment == 90);
    assert(!window.consume(0));
}

void receiveWindowResetForgetsPendingCredit() {
    Http2ReceiveWindow window(100);
    assert(!window.consume(40));
    window.reset();
    assert(!window.consume(40)); // would be 80 without the reset
}

void receiveWindowClampsIncrementToProtocolMaximum() {
    // WINDOW_UPDATE increments are 31-bit (RFC 9113 6.9.1). Larger credit
    // must be split across updates rather than wrapping or being lost.
    Http2ReceiveWindow window(0x7fffffffU);
    const auto first = window.consume(0xffffffffU);
    assert(first.has_value() && *first == 0x7fffffffU);

    const auto second = window.consume(0); // remaining 0x80000000 credit
    assert(second.has_value() && *second == 0x7fffffffU);

    // One unit of credit remains, which is far below half the window.
    assert(!window.consume(0));
}

void receiveWindowWithZeroSizeNeverStalls() {
    Http2ReceiveWindow window(0);
    assert(!window.consume(0));
    const auto increment = window.consume(1);
    assert(increment.has_value() && *increment == 1);
}

void receiveWindowSizedForTheClientAdvertisesEnoughHeadroom() {
    // With the real stream window, a netmap-sized burst well above the HTTP/2
    // default of 65535 must not require a stall before the first update.
    Http2ReceiveWindow stream(kHttp2ClientStreamWindow);
    assert(!stream.consume(kHttp2DefaultWindow * 8));
    const auto update = stream.consume(kHttp2ClientStreamWindow / 2);
    assert(update.has_value());
    assert(*update >= kHttp2ClientStreamWindow / 2);
}

} // namespace

int main() {
    windowUpdateMasksReservedBit();
    windowUpdateTargetsConnectionWithStreamZero();
    settingsAckIsExactlyTheRfcFrame();
    pingAckEchoesOpaqueDataWithAckFlag();
    clientPrefaceLayout();
    advertisedWindowsAreOrderedAndLegal();
    postHeadersMatchHpackWireFormat();
    postHeadersUseTheGivenStream();
    dataFrameFlagsAndPayload();
    builtFramesRoundTripThroughDecoder();
    decoderWaitsForCompleteFramesWhenFedOneByteAtATime();
    decoderKeepsTrailingPartialFrame();
    decoderMasksReservedStreamIdBit();
    decoderAcceptsMaximumSizedFrameAndRejectsOverflow();
    decoderDropsOversizedFrameAndRecovers();
    receiveWindowWaitsForHalfBeforeReplenishing();
    receiveWindowReturnsEverythingConsumedInOneUpdate();
    receiveWindowResetForgetsPendingCredit();
    receiveWindowClampsIncrementToProtocolMaximum();
    receiveWindowWithZeroSizeNeverStalls();
    receiveWindowSizedForTheClientAdvertisesEnoughHeadroom();
    return 0;
}
