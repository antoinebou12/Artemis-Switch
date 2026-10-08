#pragma once

#include "TailscaleCore.hpp"
#include "TailscaleTypes.hpp"

#include <array>
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

constexpr std::array<std::uint16_t, 3> kTailscaleTcpPorts{47989, 47984, 48010};
constexpr std::array<std::uint16_t, 5> kTailscaleUdpPorts{47998, 48000, 47999, 48002, 48010};

// Everything a backend needs to look for a direct UDP path to the route's
// peer. WireGuard keeps working over DERP until a path is proven by pongs.
struct DirectPathConfig {
    std::string peerStableId;
    Key32 peerNodeKey{};
    Key32 peerDiscoKey{};
    std::vector<std::string> peerEndpoints; // "ip:port" from the netmap
    Key32 localNodePrivate{};
    Key32 localDiscoPrivate{};
    std::vector<DerpRegion> derpMap;
    int derpRegion = 0;
    // Our discovered UDP endpoints, to be advertised to control.
    std::function<void(std::vector<std::string>)> publishEndpoints;
    // Path change for peerStableId: endpoint + RTT when direct, empty when
    // traffic is back on DERP.
    std::function<void(const std::string& endpoint, int rttMs)> pathChanged;
};

// ponytail: abstraction ceiling is in-process loopback proxy; upgrade to direct socket forwarding if OS TUN exists.
class IWgxBackend {
public:
    virtual ~IWgxBackend() = default;
    virtual bool startTunnel(const Key32& privateKey, const std::string& localIp,
                             std::string* error) = 0;
    virtual bool addOrUpdatePeer(uint32_t peerId, const Key32& publicKey,
                                 const std::string& peerIp, std::string* error) = 0;
    // peerIp is the WireGuard peer's tailnet address; hostIp is the GameStream
    // host the relay dials. They differ when the peer is a subnet router and
    // the host sits on the LAN behind it.
    virtual bool startTcpProxy(const std::string& peerIp,
                               const std::string& hostIp,
                               std::span<const std::uint16_t> ports,
                               std::string* error) = 0;
    virtual bool startUdpRelay(const std::string& peerIp,
                               std::span<const std::uint16_t> ports,
                               std::string* error) = 0;
    // Establishes the relayed packet path (DERP) for the peer's WireGuard
    // session. Called after addOrUpdatePeer, before startTcpProxy. Backends
    // without relay support fail closed with an explanatory error.
    virtual bool ensureDerpRoute(const Key32& peerNodeKey, int homeDerpRegion,
                                 const std::vector<DerpRegion>& derpMap,
                                 const Key32& localPrivateKey,
                                 std::string* error) {
        (void)peerNodeKey;
        (void)homeDerpRegion;
        (void)derpMap;
        (void)localPrivateKey;
        if (error)
            *error = "DERP relay is not available in this backend";
        return false;
    }
    virtual void stopUdpRelay() noexcept = 0;
    virtual void stop() noexcept = 0;
    [[nodiscard]] virtual bool isRunning() const noexcept = 0;
    // Optional direct-path discovery (STUN + disco). Called after the
    // WireGuard handshake succeeded over DERP; backends without UDP support
    // keep relaying. Never fails the route.
    virtual void startDirectPath(DirectPathConfig config) { (void)config; }
};

class SimulatedWgxBackend : public IWgxBackend {
public:
    bool startTunnel(const Key32& privateKey, const std::string& localIp,
                     std::string* error) override;
    bool addOrUpdatePeer(uint32_t peerId, const Key32& publicKey,
                         const std::string& peerIp, std::string* error) override;
    bool startTcpProxy(const std::string& peerIp, const std::string& hostIp,
                       std::span<const std::uint16_t> ports,
                       std::string* error) override;
    [[nodiscard]] const std::string& activeHostIp() const noexcept {
        return activeHostIp_;
    }
    void startDirectPath(DirectPathConfig config) override {
        directConfig_ = std::move(config);
    }
    [[nodiscard]] const std::optional<DirectPathConfig>& directConfig()
        const noexcept {
        return directConfig_;
    }
    bool startUdpRelay(const std::string& peerIp,
                       std::span<const std::uint16_t> ports,
                       std::string* error) override;
    bool ensureDerpRoute(const Key32& peerNodeKey, int homeDerpRegion,
                         const std::vector<DerpRegion>& derpMap,
                         const Key32& localPrivateKey,
                         std::string* error) override;
    void stopUdpRelay() noexcept override;
    void stop() noexcept override;
    [[nodiscard]] bool isRunning() const noexcept override;
    [[nodiscard]] bool isTcpActive() const noexcept;
    [[nodiscard]] bool isUdpActive() const noexcept;
    [[nodiscard]] bool isDerpReady() const noexcept;

private:
    bool running_ = false;
    bool tcpActive_ = false;
    bool udpActive_ = false;
    bool derpReady_ = false;
    Key32 privateKey_{};
    std::string localIp_;
    std::string activePeerIp_;
    std::string activeHostIp_;
    std::optional<DirectPathConfig> directConfig_;
    std::vector<uint16_t> tcpPorts_;
    std::vector<uint16_t> udpPorts_;
};

class TailscaleWgxRoute final : public IOverlayRoute {
public:
    using PeerResolver =
        std::function<std::optional<Peer>(std::string_view peerId)>;
    using LocalInfoProvider =
        std::function<std::optional<std::pair<std::string, Key32>>()>;
    using DerpMapProvider = std::function<std::vector<DerpRegion>()>;
    using DiscoKeyProvider = std::function<std::optional<Key32>()>;
    using EndpointPublisher = std::function<void(std::vector<std::string>)>;
    using PathObserver = std::function<void(const std::string& peerId,
                                            const std::string& endpoint,
                                            int rttMs)>;

    explicit TailscaleWgxRoute(std::shared_ptr<IWgxBackend> backend = nullptr,
                               PeerResolver peerResolver = nullptr,
                               LocalInfoProvider localInfoProvider = nullptr);

    void setPeerResolver(PeerResolver resolver);
    void setLocalInfoProvider(LocalInfoProvider provider);
    void setDerpMapProvider(DerpMapProvider provider);
    void setBackend(std::shared_ptr<IWgxBackend> backend);
    void setDirectPathHooks(DiscoKeyProvider discoKey,
                            EndpointPublisher publisher, PathObserver observer);

    bool start(const RemoteRouteTarget& target,
               std::string* error) override;
    bool prepareForStreaming(const RemoteRouteTarget& target,
                             std::string* error) override;
    void stop() noexcept override;

    [[nodiscard]] bool isActive() const noexcept;
    [[nodiscard]] bool isStreamingPrepared() const noexcept;
    [[nodiscard]] std::string activePeerId() const;

private:
    mutable std::mutex mutex_;
    std::shared_ptr<IWgxBackend> backend_;
    PeerResolver peerResolver_;
    LocalInfoProvider localInfoProvider_;
    DerpMapProvider derpMapProvider_;
    DiscoKeyProvider discoKeyProvider_;
    EndpointPublisher endpointPublisher_;
    PathObserver pathObserver_;
    std::optional<RemoteRouteTarget> activeTarget_;
    bool streamingPrepared_ = false;
};

} // namespace artemis::tailscale
