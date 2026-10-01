#include "AddHostPeerFilter.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>

namespace artemis::remote {
namespace {

std::string lower(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

} // namespace

bool isMobileOs(std::string_view os) {
    const auto value = lower(os);
    return value == "android" || value == "ios" || value == "ipados";
}

PeerVerdict classifyPeerForAddHost(const RemoteAccessPeer& peer) {
    if (!peer.online || peer.address.empty())
        return PeerVerdict::Hidden;
    if (isMobileOs(peer.os))
        return PeerVerdict::Hidden;
    // A reported GameStream port is proof enough. Its absence is not: control
    // often sends no real port list at all, so everything else is probed.
    if (peer.tcpPortsKnown &&
        std::any_of(peer.tcpPorts.begin(), peer.tcpPorts.end(),
                    [](std::uint16_t port) {
                        return port == kGameStreamHttpPort ||
                               port == kGameStreamHttpsPort;
                    }))
        return PeerVerdict::Host;
    return PeerVerdict::Probe;
}

std::string peerDisplayName(const RemoteAccessPeer& peer) {
    std::string name = peer.name.empty() ? peer.address : peer.name;
    while (!name.empty() && name.back() == '.')
        name.pop_back();
    return name.empty() ? peer.address : name;
}

std::string subnetInputPrefix(std::string_view cidr) {
    const auto slash = cidr.find('/');
    if (slash == std::string_view::npos)
        return {};
    const auto prefixText = cidr.substr(slash + 1);
    int bits = -1;
    const auto result = std::from_chars(
        prefixText.data(), prefixText.data() + prefixText.size(), bits);
    if (result.ec != std::errc{} ||
        result.ptr != prefixText.data() + prefixText.size() || bits < 8 ||
        bits > 32)
        return {};
    const int octets = std::min(bits / 8, 3);
    const auto network = cidr.substr(0, slash);
    std::string out;
    std::size_t begin = 0;
    for (int i = 0; i < octets; ++i) {
        const auto dot = network.find('.', begin);
        if (dot == std::string_view::npos)
            return {};
        out.append(network.substr(begin, dot - begin));
        out.push_back('.');
        begin = dot + 1;
    }
    return out;
}

std::vector<SubnetShortcut>
subnetShortcuts(const std::vector<RemoteAccessPeer>& peers) {
    std::vector<SubnetShortcut> out;
    for (const auto& peer : peers) {
        if (!peer.online)
            continue;
        for (const auto& subnet : peer.subnets) {
            const auto prefix = subnetInputPrefix(subnet);
            if (prefix.empty())
                continue;
            const bool seen = std::any_of(
                out.begin(), out.end(),
                [&](const SubnetShortcut& s) { return s.subnet == subnet; });
            if (!seen)
                out.push_back({subnet, prefix, peerDisplayName(peer)});
        }
    }
    return out;
}

std::optional<bool> ProbeCache::lookup(const std::string& peerId,
                                       Clock::time_point now) const {
    std::lock_guard lock(mutex_);
    const auto found = entries_.find(peerId);
    if (found == entries_.end())
        return std::nullopt;
    if (!found->second.isHost && now - found->second.at > kNegativeTtl)
        return std::nullopt;
    return found->second.isHost;
}

void ProbeCache::store(const std::string& peerId, bool isHost,
                       Clock::time_point now) {
    std::lock_guard lock(mutex_);
    entries_[peerId] = {isHost, now};
}

void ProbeCache::forgetMisses() {
    std::lock_guard lock(mutex_);
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (it->second.isHost)
            ++it;
        else
            it = entries_.erase(it);
    }
}

void ProbeCache::clear() {
    std::lock_guard lock(mutex_);
    entries_.clear();
}

} // namespace artemis::remote
