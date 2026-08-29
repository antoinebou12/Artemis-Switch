#include "TailscaleControlSession.hpp"

#include "TailscaleControlCodec.hpp"
#include "TailscaleHttpUpgrade.hpp"
#include "TailscaleTypes.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <random>
#include <utility>
#include <vector>

namespace artemis::tailscale {
namespace {

constexpr std::size_t kMaxResponseHeader = 16 * 1024;
constexpr std::size_t kReadChunk = 2048;

bool readHttpHeader(ITransport& transport, std::string* header,
                    std::string* error) {
    header->clear();
    std::array<std::uint8_t, 1> byte{};
    while (header->size() < kMaxResponseHeader) {
        const int received = transport.read(byte.data(), 1, error);
        if (received < 0) return false;
        if (received == 0) {
            if (error) *error = "control connection closed during HTTP upgrade";
            return false;
        }
        header->push_back(static_cast<char>(byte[0]));
        if (header->ends_with("\r\n\r\n")) return true;
    }
    if (error) *error = "oversized HTTP upgrade response";
    return false;
}

bool readExact(ITransport& transport, std::span<std::uint8_t> output,
               std::string* error) {
    std::size_t offset = 0;
    while (offset < output.size()) {
        const int received =
            transport.read(output.data() + offset, output.size() - offset, error);
        if (received < 0) return false;
        if (received == 0) {
            if (error) *error = "control connection closed mid-handshake";
            return false;
        }
        offset += static_cast<std::size_t>(received);
    }
    return true;
}

} // namespace

TailscaleControlSession::TailscaleControlSession(
    std::function<std::unique_ptr<ITransport>()> transportFactory,
    std::string host, std::uint16_t port, Key32 controlPublic,
    std::string hostname, RecordReader recordReader)
    : transportFactory_(std::move(transportFactory)), host_(std::move(host)),
      port_(port), controlPublic_(controlPublic),
      hostname_(std::move(hostname)), recordReader_(std::move(recordReader)) {
    // Only live (non-injected) sessions need the concrete POSIX transport.
    if (!recordReader_ && !transportFactory_) {
        transportFactory_ = [] { return std::make_unique<TcpTransport>(); };
    }
}

bool TailscaleControlSession::connect(const Identity& identity,
                                      std::span<const std::uint8_t> authKey,
                                      std::string* error) {
    transport_.reset();
    noise_.reset();
    plaintextQueue_.clear();
    ready_ = false;

    // An injected record reader makes this a pure feed-in test or adapter
    // seam: no network is needed, records arrive as decrypted plaintext.
    if (recordReader_) {
        ready_ = true;
        return true;
    }

    const bool unconfigured = host_.empty() || port_ == 0 ||
                              std::all_of(controlPublic_.begin(),
                                          controlPublic_.end(),
                                          [](std::uint8_t byte) {
                                              return byte == 0;
                                          });
    if (unconfigured) {
        if (error) *error = "Tailscale control endpoint is not configured";
        return false;
    }
    if (!transportFactory_) {
        if (error) *error = "Tailscale transport is unavailable";
        return false;
    }
    transport_ = transportFactory_();
    if (!transport_ || !transport_->connect(host_, port_, error)) {
        transport_.reset();
        if (error && error->empty()) *error = "cannot reach Tailscale control";
        return false;
    }

    Key32 ephemeral{};
    {
        std::random_device source;
        for (auto& byte : ephemeral) byte = static_cast<std::uint8_t>(source());
    }
    noise_ = std::make_unique<NoiseClient>(identity.machinePrivate,
                                           controlPublic_, ephemeral);

    std::vector<std::uint8_t> initiation;
    if (!noise_->begin(&initiation, error)) {
        transport_->close();
        transport_.reset();
        return false;
    }
    const auto request = buildTs2021UpgradeRequest(host_, initiation);
    if (request.empty()) {
        if (error) *error = "cannot build TS2021 upgrade request";
        transport_->close();
        transport_.reset();
        return false;
    }
    if (!transport_->write(std::span<const std::uint8_t>(
                               reinterpret_cast<const std::uint8_t*>(request.data()),
                               request.size()),
                           error)) {
        transport_->close();
        transport_.reset();
        return false;
    }
    std::string header;
    if (!readHttpHeader(*transport_, &header, error) ||
        !validateTs2021UpgradeResponse(header, error)) {
        transport_->close();
        transport_.reset();
        return false;
    }
    std::array<std::uint8_t, NoiseClient::kResponseSize> response{};
    if (!readExact(*transport_, response, error)) {
        transport_->close();
        transport_.reset();
        if (error && error->empty()) *error = "incomplete TS2021 handshake";
        return false;
    }
    if (!noise_->complete(response, error)) {
        transport_->close();
        transport_.reset();
        return false;
    }

    (void)authKey;
    if (error)
        *error = "Tailscale HTTP/2-over-Noise control transport is not implemented";
    transport_->close();
    transport_.reset();
    noise_.reset();
    return false;
}

bool TailscaleControlSession::readNextNoiseRecord(std::string* record,
                                                  std::string* error) {
    while (plaintextQueue_.empty()) {
        std::array<std::uint8_t, kReadChunk> buffer{};
        const int received =
            transport_->read(buffer.data(), buffer.size(), error);
        if (received < 0) return false;
        if (received == 0) {
            if (error) *error = "control connection closed";
            return false;
        }
        if (!noise_ ||
            !noise_->consume(std::span<const std::uint8_t>(
                                 buffer.data(), static_cast<std::size_t>(received)),
                             &plaintextQueue_, error))
            return false;
    }
    const auto& first = plaintextQueue_.front();
    record->assign(first.begin(), first.end());
    plaintextQueue_.erase(plaintextQueue_.begin());
    return true;
}

bool TailscaleControlSession::poll(PeerDelta* delta,
                                   std::optional<std::vector<Peer>>* fullPeers,
                                   std::string* localAddress, std::string* error) {
    if (!ready_ || !recordReader_ || !delta || !fullPeers || !localAddress) {
        if (error) *error = "Tailscale control session is not connected";
        return false;
    }
    if (fullPeers->has_value()) fullPeers->reset();
    delta->changed.clear();
    delta->removedStableIds.clear();
    localAddress->clear();

    std::string record;
    if (!recordReader_(&record, error)) return false;

    auto update = mapCodec_.decode(record, error);
    if (!update) return false;
    if (update->keepAlive) return true;
    if (update->fullPeers) *fullPeers = std::move(*update->fullPeers);
    if (!update->localAddress.empty()) *localAddress = update->localAddress;
    delta->changed = std::move(update->delta.changed);
    delta->removedStableIds = std::move(update->delta.removedStableIds);
    return true;
}

void TailscaleControlSession::close() noexcept {
    if (transport_) transport_->close();
    transport_.reset();
    noise_.reset();
    plaintextQueue_.clear();
    ready_ = false;
    recordReader_ = nullptr;
}

} // namespace artemis::tailscale
