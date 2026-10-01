#pragma once

#include "TailscaleControlCodec.hpp"
#include "TailscaleCore.hpp"
#include "TailscaleNoise.hpp"
#include "TailscaleHttp2.hpp"
#include "TailscaleTransport.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
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
              std::string* localAddress,
              std::optional<std::vector<DerpRegion>>* derpMap,
              std::string* error) override;
    bool sendHostinfoUpdate(int preferredDerp, std::string* error) override;
    void setInitialPreferredDerp(int region) noexcept override;
    void setLocalEndpoints(std::vector<std::string> endpoints) override;
    void close() noexcept override;
    void interrupt() noexcept override;

private:
    void resetTransport() noexcept;
    bool readNextNoiseRecord(std::string* record, std::string* error);
    // Diagnostics only: logs HTTP/2 status, resets, GOAWAY, and receive
    // window usage so vpn.log shows why a control stream stopped.
    void traceFrame(const Http2Frame& frame);
    // Replenishes the connection and map-stream receive windows for a DATA
    // frame. Without this the server stops sending once the window is used
    // up and the long poll stalls until the connection drops.
    bool creditData(const Http2Frame& frame, std::string* error);
    void logRegisterResponse(std::span<const std::uint8_t> payload);

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
    MapFrameDecoder mapFrameDecoder_;
    Http2FrameDecoder http2Decoder_;
    std::string dataAccumulator_;
    bool ready_ = false;
    // Kept from connect() for later non-streaming map updates.
    Key32 discoPublic_{};
    int capabilityVersion_ = 0;
    std::uint32_t nextStreamId_ = 1;
    // After connect(), writes come from the poll thread (SETTINGS/PING acks)
    // and from route activation (Hostinfo updates). Serializes the Noise tx
    // nonce and the TLS write; reads stay on the poll thread only.
    std::mutex writeMutex_;
    // Guards the transport_ pointer between close() on the poll thread and
    // interrupt() from stop() on the UI side.
    std::mutex transportMutex_;
    // Diagnostics state (see traceFrame).
    Key32 nodePublic_{};
    std::uint32_t registerStreamId_ = 0;
    std::uint32_t mapStreamId_ = 0;
    int registerHttpStatus_ = 0;
    int mapHttpStatus_ = 0;
    std::uint64_t dataBytesReceived_ = 0;
    bool windowWarned_ = false;
    std::uint64_t windowUpdatesSent_ = 0;
    Http2ReceiveWindow connectionWindow_{kHttp2ClientConnectionWindow};
    Http2ReceiveWindow mapStreamWindow_{kHttp2ClientStreamWindow};
    int initialPreferredDerp_ = 0;
    std::mutex endpointsMutex_;
    std::vector<std::string> localEndpoints_; // guarded by endpointsMutex_
    std::vector<std::string> localEndpoints();
};

} // namespace artemis::tailscale
