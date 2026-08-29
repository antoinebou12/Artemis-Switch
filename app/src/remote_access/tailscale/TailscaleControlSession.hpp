#pragma once

#include "TailscaleControlCodec.hpp"
#include "TailscaleCore.hpp"
#include "TailscaleNoise.hpp"
#include "TailscaleTransport.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace artemis::tailscale {

// Client half of the TS2021 control channel on an injected byte-stream.
// The Noise handshake and HTTP upgrade run over ITransport;
// the decrypted map stream is consumed through an injectable RecordReader so
// the whole poll/contract path is unit-testable without server-side crypto.
// When no RecordReader is supplied, one is built on the Noise session so the
// class is ready for the required HTTP/2 adapter. Until that adapter is
// installed, live connect fails explicitly after Noise rather than emitting an
// invalid raw-JSON registration record.
class TailscaleControlSession final : public IControlSession {
public:
    using RecordReader =
        std::function<bool(std::string* record, std::string* error)>;

    TailscaleControlSession(
        std::function<std::unique_ptr<ITransport>()> transportFactory,
        std::string host, std::uint16_t port, Key32 controlPublic,
        std::string hostname, RecordReader recordReader = {});

    bool connect(const Identity& identity,
                 std::span<const std::uint8_t> authKey,
                 std::string* error) override;
    bool poll(PeerDelta* delta, std::optional<std::vector<Peer>>* fullPeers,
              std::string* localAddress, std::string* error) override;
    void close() noexcept override;

private:
    bool readNextNoiseRecord(std::string* record, std::string* error);

    std::function<std::unique_ptr<ITransport>()> transportFactory_;
    std::string host_;
    std::uint16_t port_;
    Key32 controlPublic_;
    std::string hostname_;
    std::unique_ptr<ITransport> transport_;
    std::unique_ptr<NoiseClient> noise_;
    RecordReader recordReader_;
    std::vector<std::vector<std::uint8_t>> plaintextQueue_;
    MapCodec mapCodec_;
    bool ready_ = false;
};

} // namespace artemis::tailscale
