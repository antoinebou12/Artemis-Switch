
#if defined(__SWITCH__)
#include "../../utils/Settings.hpp"
#include "../../vpn/VpnFileLogger.hpp"
namespace {
void logTsSession(VpnFileLogger::Severity severity, std::string_view message) {
    VpnFileLogger::append(Settings::instance().working_dir() + "/vpn.log",
                          "TS", severity, message);
}
}
#define LOG_SESSION_INFO(msg) logTsSession(VpnFileLogger::Severity::Info, msg)
#define LOG_SESSION_WARN(msg) logTsSession(VpnFileLogger::Severity::Warning, msg)
#define LOG_SESSION_ERROR(msg) logTsSession(VpnFileLogger::Severity::Error, msg)
#else
#define LOG_SESSION_INFO(msg) do { (void)(msg); } while(0)
#define LOG_SESSION_WARN(msg) do { (void)(msg); } while(0)
#define LOG_SESSION_ERROR(msg) do { (void)(msg); } while(0)
#endif
#include "TailscaleControlSession.hpp"

#include "TailscaleControlCodec.hpp"
#include "TailscaleHttpUpgrade.hpp"
#include "TailscaleHttp2.hpp"
#include "TailscaleRandom.hpp"
#include "TailscaleTypes.hpp"

#if defined(__SWITCH__)
#include <borealis/extern/nlohmann/json.hpp>
#endif

extern "C" {
#include <monocypher.h>
}

#if defined(__SWITCH__)
extern "C" {
void tailscale_internal_crypto_x25519_public_key(uint8_t[32], const uint8_t[32]);
}
#define TS_CONTROL_X25519_PUB tailscale_internal_crypto_x25519_public_key
#else
#define TS_CONTROL_X25519_PUB crypto_x25519_public_key
#endif

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


bool isCompleteJsonObject(std::string_view s) {
    int depth = 0;
    bool inString = false;
    bool escape = false;
    bool foundStart = false;
    for (char c : s) {
        if (escape) {
            escape = false;
            continue;
        }
        if (c == '\\') {
            escape = true;
            continue;
        }
        if (c == '"') {
            inString = !inString;
            continue;
        }
        if (inString)
            continue;
        if (c == '{') {
            depth++;
            foundStart = true;
        } else if (c == '}') {
            depth--;
            if (foundStart && depth == 0)
                return true;
        }
    }
    return false;
}

// ---- Diagnostics helpers (log text only; never influence protocol flow) ----

[[maybe_unused]] std::string hexBytes(const std::uint8_t* bytes,
                                      std::size_t count) {
    constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(count * 2);
    for (std::size_t i = 0; i < count; ++i) {
        out.push_back(kHex[bytes[i] >> 4U]);
        out.push_back(kHex[bytes[i] & 0x0fU]);
    }
    return out;
}

[[maybe_unused]] std::string hexPrefix(const std::uint8_t* bytes,
                                       std::size_t count = 8) {
    return hexBytes(bytes, count) + "...";
}

// Printable excerpt of a body that is expected to be text (error pages,
// JSON). Control characters are replaced so one body stays one log line.
[[maybe_unused]] std::string printableExcerpt(std::span<const std::uint8_t> bytes,
                                              std::size_t limit = 240) {
    std::string out;
    const auto count = std::min(bytes.size(), limit);
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto c = bytes[i];
        out.push_back(c >= 0x20 && c < 0x7f ? static_cast<char>(c) : '.');
    }
    if (bytes.size() > limit)
        out += "...";
    return out;
}

// Decodes an HPACK Huffman string that only contains digits (":status").
// Codes from RFC 7541 Appendix B: '0'-'2' are 5 bits, '3'-'9' are 6 bits.
std::string decodeHuffmanDigits(std::span<const std::uint8_t> bytes) {
    std::string out;
    const std::size_t totalBits = bytes.size() * 8;
    std::size_t bit = 0;
    auto peek = [&](std::size_t width) {
        std::uint32_t value = 0;
        for (std::size_t i = 0; i < width; ++i) {
            const std::size_t at = bit + i;
            const std::uint32_t b =
                (bytes[at / 8] >> (7U - static_cast<unsigned>(at % 8))) & 1U;
            value = (value << 1U) | b;
        }
        return value;
    };
    while (bit + 5 <= totalBits && out.size() < 3) {
        const auto five = peek(5);
        if (five <= 2) {
            out.push_back(static_cast<char>('0' + five));
            bit += 5;
            continue;
        }
        if (bit + 6 > totalBits)
            break;
        const auto six = peek(6);
        if (six >= 0x19 && six <= 0x1f) {
            out.push_back(static_cast<char>('3' + (six - 0x19)));
            bit += 6;
            continue;
        }
        break;
    }
    return out;
}

// Best-effort ":status" extraction from the first header field of a HEADERS
// frame. Servers always emit :status first. Returns 0 when not decodable.
int http2ResponseStatus(const Http2Frame& frame) {
    const auto& p = frame.payload;
    std::size_t offset = 0;
    if (frame.flags & 0x08) // PADDED
        offset += 1;
    if (frame.flags & 0x20) // PRIORITY
        offset += 5;
    // Skip leading HPACK dynamic table size updates (001xxxxx, with a
    // multi-byte integer when the 5-bit prefix is all ones). Go's server
    // emits one at the start of the first header block on a connection.
    while (offset < p.size() && (p[offset] & 0xe0) == 0x20) {
        const bool multiByte = (p[offset] & 0x1f) == 0x1f;
        ++offset;
        if (multiByte) {
            while (offset < p.size() && (p[offset] & 0x80))
                ++offset;
            ++offset;
        }
    }
    if (offset >= p.size())
        return 0;
    const std::uint8_t first = p[offset];
    if (first & 0x80) { // indexed field
        switch (first & 0x7f) {
        case 8: return 200;
        case 9: return 204;
        case 10: return 206;
        case 11: return 304;
        case 12: return 400;
        case 13: return 404;
        case 14: return 500;
        default: return 0;
        }
    }
    // Literal with name index (incremental 01xxxxxx, without 0000xxxx,
    // never-indexed 0001xxxx). Static indexes 8..14 are all ":status".
    unsigned nameIndex = 0;
    if ((first & 0xc0) == 0x40)
        nameIndex = first & 0x3f;
    else if ((first & 0xf0) == 0x00 || (first & 0xf0) == 0x10)
        nameIndex = first & 0x0f;
    if (nameIndex < 8 || nameIndex > 14 || offset + 1 >= p.size())
        return 0;
    const std::uint8_t lengthByte = p[offset + 1];
    const bool huffman = (lengthByte & 0x80) != 0;
    const std::size_t length = lengthByte & 0x7f;
    if (offset + 2 + length > p.size())
        return 0;
    const std::span<const std::uint8_t> value(p.data() + offset + 2, length);
    std::string digits =
        huffman ? decodeHuffmanDigits(value)
                : std::string(reinterpret_cast<const char*>(value.data()),
                              value.size());
    int status = 0;
    for (const char c : digits) {
        if (c < '0' || c > '9')
            return 0;
        status = status * 10 + (c - '0');
    }
    return status;
}

#if defined(__SWITCH__)
// Type-checked field readers: control may send null for optional fields and
// a throwing nlohmann accessor must never take the control loop down.
bool jsonBool(const nlohmann::json& object, const char* key, bool fallback = false) {
    const auto it = object.find(key);
    return it != object.end() && it->is_boolean() ? it->get<bool>() : fallback;
}

int jsonInt(const nlohmann::json& object, const char* key) {
    const auto it = object.find(key);
    return it != object.end() && it->is_number_integer() ? it->get<int>() : 0;
}

std::string jsonString(const nlohmann::json& object, const char* key) {
    const auto it = object.find(key);
    return it != object.end() && it->is_string() ? it->get<std::string>()
                                                 : std::string{};
}

std::size_t jsonArraySize(const nlohmann::json& object, const char* key) {
    const auto it = object.find(key);
    return it != object.end() && it->is_array() ? it->size() : 0;
}

bool jsonHasArray(const nlohmann::json& object, const char* key) {
    const auto it = object.find(key);
    return it != object.end() && it->is_array();
}

// HomeDERP (new) or the legacy "127.3.3.40:<region>" DERP string.
int nodeHomeDerp(const nlohmann::json& node) {
    int region = jsonInt(node, "HomeDERP");
    if (region != 0)
        return region;
    const auto legacy = jsonString(node, "DERP");
    const auto colon = legacy.rfind(':');
    if (colon == std::string::npos)
        return 0;
    for (std::size_t i = colon + 1; i < legacy.size(); ++i) {
        if (legacy[i] < '0' || legacy[i] > '9')
            return 0;
        region = region * 10 + (legacy[i] - '0');
    }
    return region;
}

std::string joinAddresses(const nlohmann::json& node) {
    std::string out;
    const auto it = node.find("Addresses");
    if (it == node.end() || !it->is_array())
        return "none";
    for (const auto& value : *it) {
        if (!value.is_string())
            continue;
        if (!out.empty())
            out += ",";
        out += value.get<std::string>();
    }
    return out.empty() ? "none" : out;
}

std::string shortTypedKey(const std::string& typed) {
    const auto colon = typed.find(':');
    const auto hex = colon == std::string::npos ? typed : typed.substr(colon + 1);
    return hex.size() > 16 ? hex.substr(0, 16) + "..." : hex;
}

void logNetmapDiagnostics(const nlohmann::json& root, const Key32& nodePublic) {
    if (jsonBool(root, "KeepAlive"))
        return;

    // Self node: exactly what every peer is told about this Switch.
    if (const auto node = root.find("Node");
        node != root.end() && node->is_object()) {
        const int homeDerp = nodeHomeDerp(*node);
        const auto key = jsonString(*node, "Key");
        const bool keyMatches =
            key == "nodekey:" + hexBytes(nodePublic.data(), nodePublic.size());
        // omitempty in tailcfg.Node: absent means false, as tailscaled reads it.
        const bool authorized = jsonBool(*node, "MachineAuthorized");
        const bool expired = jsonBool(*node, "Expired");
        const auto keyExpiry = jsonString(*node, "KeyExpiry");
        LOG_SESSION_INFO(
            "self node: name=" + jsonString(*node, "Name") +
            " addresses=" + joinAddresses(*node) +
            " homeDERP=" + std::to_string(homeDerp) +
            " machineAuthorized=" + (authorized ? "yes" : "no") +
            " expired=" + (expired ? "yes" : "no") +
            " keyExpiry=" + (keyExpiry.empty() ? "none" : keyExpiry) +
            " nodekeyMatchesLocal=" + (keyMatches ? "yes" : "NO"));
        if (!keyMatches)
            LOG_SESSION_ERROR(
                "control's Node.Key differs from the local nodekey: the "
                "netmap belongs to another key, DERP and WireGuard will use "
                "a key nobody knows");
        if (!authorized)
            LOG_SESSION_ERROR(
                "this node is not authorized: approve it in the admin "
                "console; until then peers do not accept its packets");
        if (expired)
            LOG_SESSION_ERROR("this node's key has expired: re-register it");
        if (homeDerp == 0)
            LOG_SESSION_WARN(
                "control reports homeDERP=0 for this node: peers have no "
                "DERP region to reply to, so WireGuard responses will not "
                "come back");
    }

    // Peer lists: is the streaming host visible, online, and relayable?
    // Control may send the initial list as "Peers" or as "PeersChanged".
    for (const char* listKey : {"Peers", "PeersChanged"}) {
        if (!jsonHasArray(root, listKey))
            continue;
        const auto& peers = root[listKey];
        LOG_SESSION_INFO(std::string("netmap ") + listKey + ": " +
                         std::to_string(peers.size()));
        if (peers.empty() && std::string_view(listKey) == "Peers")
            LOG_SESSION_WARN(
                "control sent 0 peers: ACLs/grants hide every device from "
                "this node, or the host is in another tailnet");
        constexpr std::size_t kMaxPeerLines = 24;
        std::size_t index = 0;
        for (const auto& peer : peers) {
            if (!peer.is_object())
                continue;
            if (index >= kMaxPeerLines) {
                LOG_SESSION_INFO("... " +
                                 std::to_string(peers.size() - kMaxPeerLines) +
                                 " more peers not listed");
                break;
            }
            const auto online = peer.find("Online");
            const std::string onlineText =
                online != peer.end() && online->is_boolean()
                    ? (online->get<bool>() ? "yes" : "no")
                    : "unknown";
            LOG_SESSION_INFO(
                "peer[" + std::to_string(index) + "] name=" +
                jsonString(peer, "Name") + " addresses=" +
                joinAddresses(peer) + " online=" + onlineText +
                " homeDERP=" + std::to_string(nodeHomeDerp(peer)) +
                " endpoints=" + std::to_string(jsonArraySize(peer, "Endpoints")) +
                " disco=" + (jsonString(peer, "DiscoKey").empty() ? "no" : "yes") +
                " nodekey=" + shortTypedKey(jsonString(peer, "Key")));
            ++index;
        }
    }

    // Deltas: shows whether the stream keeps delivering updates.
    const auto changed = jsonArraySize(root, "PeersChanged");
    const auto removed = jsonArraySize(root, "PeersRemoved");
    const auto patched = jsonArraySize(root, "PeersChangedPatch");
    if (changed || removed || patched)
        LOG_SESSION_INFO("netmap delta: changed=" + std::to_string(changed) +
                         " removed=" + std::to_string(removed) +
                         " patched=" + std::to_string(patched));

    if (jsonHasArray(root, "PacketFilter"))
        LOG_SESSION_INFO("netmap PacketFilter rules=" +
                         std::to_string(root["PacketFilter"].size()));

    // Control-side health warnings (e.g. key expiry, blocked features).
    if (jsonHasArray(root, "Health")) {
        std::size_t shown = 0;
        for (const auto& item : root["Health"]) {
            if (!item.is_string() || shown >= 5)
                continue;
            LOG_SESSION_WARN("control health: " + item.get<std::string>());
            ++shown;
        }
    }
}
#endif

} // namespace

void TailscaleControlSession::traceFrame(const Http2Frame& frame) {
    switch (frame.type) {
    case 0x00: { // DATA
        dataBytesReceived_ += frame.payload.size();
        if (!windowWarned_ && dataBytesReceived_ > kHttp2DefaultWindow) {
            // Past the point where the old client (no WINDOW_UPDATE) stalled.
            windowWarned_ = true;
            LOG_SESSION_INFO("HTTP/2 control stream passed " +
                             std::to_string(kHttp2DefaultWindow) +
                             " bytes; receive windows are being replenished");
        }
        const bool onRegister = frame.streamId == registerStreamId_ &&
                                registerStreamId_ != 0;
        const int status = onRegister ? registerHttpStatus_ : mapHttpStatus_;
        if (status != 0 && status != 200) {
            LOG_SESSION_ERROR(
                std::string(onRegister ? "register" : "map") +
                " response body (HTTP " + std::to_string(status) + "): " +
                printableExcerpt(frame.payload));
        }
        break;
    }
    case 0x01: { // HEADERS
        const int status = http2ResponseStatus(frame);
        const bool onRegister = frame.streamId == registerStreamId_ &&
                                registerStreamId_ != 0;
        const std::string which =
            onRegister ? "register"
                       : (frame.streamId == mapStreamId_ ? "map"
                                                         : "hostinfo-update");
        if (onRegister)
            registerHttpStatus_ = status;
        else if (frame.streamId == mapStreamId_)
            mapHttpStatus_ = status;
        const std::string statusText =
            status == 0 ? "undecoded" : std::to_string(status);
        if (status == 200) {
            LOG_SESSION_INFO("HTTP/2 " + which + " stream " +
                             std::to_string(frame.streamId) + " status 200");
        } else if (status == 0) {
            LOG_SESSION_WARN("HTTP/2 " + which + " stream " +
                             std::to_string(frame.streamId) +
                             " status could not be decoded (header block "
                             "starts 0x" +
                             hexBytes(frame.payload.data(),
                                      std::min<std::size_t>(frame.payload.size(), 4)) +
                             "); the response body decides success");
        } else {
            LOG_SESSION_ERROR(
                "HTTP/2 " + which + " stream " +
                std::to_string(frame.streamId) + " status " + statusText +
                " (control rejected the request; body follows if any)");
        }
        break;
    }
    case 0x03: { // RST_STREAM
        std::uint32_t code = 0;
        if (frame.payload.size() >= 4)
            code = (static_cast<std::uint32_t>(frame.payload[0]) << 24U) |
                   (static_cast<std::uint32_t>(frame.payload[1]) << 16U) |
                   (static_cast<std::uint32_t>(frame.payload[2]) << 8U) |
                   frame.payload[3];
        LOG_SESSION_ERROR("HTTP/2 RST_STREAM on stream " +
                          std::to_string(frame.streamId) + " error code " +
                          std::to_string(code) +
                          " (1=protocol 2=internal 3=flow-control 8=cancel "
                          "11=enhance-your-calm); this stream is dead");
        break;
    }
    case 0x07: { // GOAWAY
        std::uint32_t lastStream = 0;
        std::uint32_t code = 0;
        const auto& p = frame.payload;
        if (p.size() >= 8) {
            lastStream = ((static_cast<std::uint32_t>(p[0]) & 0x7fU) << 24U) |
                         (static_cast<std::uint32_t>(p[1]) << 16U) |
                         (static_cast<std::uint32_t>(p[2]) << 8U) | p[3];
            code = (static_cast<std::uint32_t>(p[4]) << 24U) |
                   (static_cast<std::uint32_t>(p[5]) << 16U) |
                   (static_cast<std::uint32_t>(p[6]) << 8U) | p[7];
        }
        const std::span<const std::uint8_t> debug =
            p.size() > 8 ? std::span<const std::uint8_t>(p.data() + 8,
                                                         p.size() - 8)
                         : std::span<const std::uint8_t>{};
        LOG_SESSION_ERROR("HTTP/2 GOAWAY from control: last stream " +
                          std::to_string(lastStream) + " error code " +
                          std::to_string(code) + " debug=\"" +
                          printableExcerpt(debug) + "\"");
        break;
    }
    default:
        break;
    }
}

bool TailscaleControlSession::creditData(const Http2Frame& frame,
                                        std::string* error) {
    if (frame.type != 0x00 || frame.length == 0 || !noise_ || !transport_)
        return true;
    // Every DATA byte (padding included) counts against the connection
    // window; only the long-lived map stream needs its own stream window
    // replenished. Other streams are short replies that end on their own.
    std::vector<std::vector<std::uint8_t>> updates;
    if (const auto increment = connectionWindow_.consume(frame.length))
        updates.push_back(buildHttp2WindowUpdate(0, *increment));
    const bool streamEnded = (frame.flags & 0x01) != 0;
    if (frame.streamId == mapStreamId_ && mapStreamId_ != 0 && !streamEnded) {
        if (const auto increment = mapStreamWindow_.consume(frame.length))
            updates.push_back(buildHttp2WindowUpdate(mapStreamId_, *increment));
    }
    if (updates.empty())
        return true;
    std::lock_guard writeLock(writeMutex_);
    for (const auto& update : updates) {
        std::vector<std::uint8_t> framed;
        if (!noise_->frame(update, &framed, error) ||
            !transport_->write(framed, error))
            return false;
    }
    ++windowUpdatesSent_;
    if (windowUpdatesSent_ == 1)
        LOG_SESSION_INFO("sent first HTTP/2 WINDOW_UPDATE after " +
                         std::to_string(dataBytesReceived_) +
                         " control bytes");
    return true;
}

void TailscaleControlSession::setInitialPreferredDerp(int region) noexcept {
    initialPreferredDerp_ = region > 0 ? region : 0;
}

void TailscaleControlSession::logRegisterResponse(
    std::span<const std::uint8_t> payload) {
#if defined(__SWITCH__)
    const std::string text(payload.begin(), payload.end());
    const auto json = nlohmann::json::parse(text, nullptr, false);
    if (!json.is_object()) {
        LOG_SESSION_WARN("register response is not a complete JSON object (" +
                         std::to_string(payload.size()) + " bytes): " +
                         printableExcerpt(payload, 120));
        return;
    }
    const bool authorized = json.value("MachineAuthorized", false);
    const bool expired = json.value("NodeKeyExpired", false);
    const std::string authUrl = json.value("AuthURL", std::string{});
    const std::string err = json.value("Error", std::string{});
    std::string line = "register result: MachineAuthorized=";
    line += authorized ? "yes" : "no";
    line += " NodeKeyExpired=";
    line += expired ? "yes" : "no";
    line += " interactiveLoginRequired=";
    line += authUrl.empty() ? "no" : "yes";
    if (!err.empty())
        line += " Error=\"" + err + "\"";
    LOG_SESSION_INFO(line);
    if (!err.empty())
        LOG_SESSION_ERROR("control refused registration: " + err);
    else if (!authUrl.empty())
        LOG_SESSION_ERROR(
            "control wants an interactive browser login for this node; the "
            "login key was not accepted (used already, expired, or not "
            "reusable). Generate a new one in the admin console");
    else if (!authorized)
        LOG_SESSION_WARN(
            "node registered but not authorized yet: approve the device in "
            "the admin console (device approval is enabled on this tailnet)");
    if (expired)
        LOG_SESSION_ERROR("node key reported expired by control");
#else
    (void)payload;
#endif
}

TailscaleControlSession::TailscaleControlSession(
    std::function<std::unique_ptr<ITransport>()> transportFactory,
    std::string host, std::uint16_t port, Key32 controlPublic,
    std::string hostname, RecordReader recordReader)
    : transportFactory_(std::move(transportFactory)), host_(std::move(host)),
      port_(port), controlPublic_(controlPublic),
      hostname_(std::move(hostname)), recordReader_(std::move(recordReader)) {
    if (!recordReader_ && !transportFactory_) {
        transportFactory_ = [] { return std::make_unique<TcpTransport>(); };
    }
}

bool TailscaleControlSession::connect(const Identity& identity,
                                      std::span<const std::uint8_t> authKey,
                                      std::string* error) {
    resetTransport();
    noise_.reset();
    plaintextQueue_.clear();
    dataAccumulator_.clear();
    ready_ = false;
    registerStreamId_ = 0;
    mapStreamId_ = 0;
    registerHttpStatus_ = 0;
    mapHttpStatus_ = 0;
    dataBytesReceived_ = 0;
    windowWarned_ = false;
    windowUpdatesSent_ = 0;
    connectionWindow_.reset();
    mapStreamWindow_.reset();
    // A reconnect starts a new HTTP/2 connection and a new full netmap:
    // nothing buffered from the dropped one may leak into it.
    http2Decoder_ = Http2FrameDecoder{};
    mapFrameDecoder_ = MapFrameDecoder{};
    mapCodec_ = MapCodec{};
    nextStreamId_ = 1;

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
    LOG_SESSION_INFO("Connecting to control host: " + host_ + ":" + std::to_string(port_));
    {
        auto transport = transportFactory_();
        std::lock_guard lock(transportMutex_);
        transport_ = std::move(transport);
    }
    if (!transport_ || !transport_->connect(host_, port_, error)) {
        LOG_SESSION_ERROR("Transport connection failed: " + (error ? *error : ""));
        resetTransport();
        if (error && error->empty()) *error = "cannot reach Tailscale control";
        return false;
    }

    Key32 ephemeral{};
    secureRandomBytes(ephemeral);
    noise_ = std::make_unique<NoiseClient>(identity.machinePrivate,
                                           controlPublic_, ephemeral);

    std::vector<std::uint8_t> initiation;
    if (!noise_->begin(&initiation, error)) {
        transport_->close();
        resetTransport();
        return false;
    }
    const auto request = buildTs2021UpgradeRequest(host_, initiation);
    if (request.empty()) {
        if (error) *error = "cannot build TS2021 upgrade request";
        transport_->close();
        resetTransport();
        return false;
    }
    if (!transport_->write(std::span<const std::uint8_t>(
                               reinterpret_cast<const std::uint8_t*>(request.data()),
                               request.size()),
                           error)) {
        transport_->close();
        resetTransport();
        return false;
    }
    std::string header;
    if (!readHttpHeader(*transport_, &header, error) ||
        !validateTs2021UpgradeResponse(header, error)) {
        transport_->close();
        resetTransport();
        return false;
    }
    std::array<std::uint8_t, NoiseClient::kResponseSize> response{};
    if (!readExact(*transport_, response, error)) {
        transport_->close();
        resetTransport();
        if (error && error->empty()) *error = "incomplete TS2021 handshake";
        return false;
    }
    LOG_SESSION_INFO("TS2021 Noise handshake completed.");
    if (!noise_->complete(response, error)) {
        transport_->close();
        resetTransport();
        return false;
    }

    Key32 nodePublic{};
    TS_CONTROL_X25519_PUB(nodePublic.data(), identity.nodePrivate.data());

    Key32 discoPublic{};
    TS_CONTROL_X25519_PUB(discoPublic.data(), identity.discoPrivate.data());
    nodePublic_ = nodePublic;
    discoPublic_ = discoPublic;

    const bool hasAuthKey = !authKey.empty();
    const int capVer = compat::kCandidateCapabilityVersion > 0
                           ? compat::kCandidateCapabilityVersion
                           : 68;
    capabilityVersion_ = capVer;
    LOG_SESSION_INFO("local identity: nodekey=" + hexPrefix(nodePublic.data()) +
                     " discokey=" + hexPrefix(discoPublic.data()) +
                     " capVer=" + std::to_string(capVer) +
                     " (compare nodekey with the admin console machine page)");
    LOG_SESSION_INFO(hasAuthKey
                         ? "login: registering with a one-off login key"
                         : "login: no login key supplied; relying on an earlier "
                           "registration of this nodekey (fails if it was never "
                           "registered or has expired)");

    // 1. Send HTTP/2 connection preface + initial SETTINGS frame
    const auto preface = buildHttp2ClientPreface();
    std::vector<std::uint8_t> framed;
    if (!noise_->frame(preface, &framed, error) ||
        !transport_->write(framed, error)) {
        transport_->close();
        resetTransport();
        return false;
    }

    std::uint32_t streamId = 1;
    if (hasAuthKey) {
        // Register the machine with Tailscale
        RegisterRequestData regData;
        regData.nodePublic = nodePublic;
        regData.authKey = std::string(
            reinterpret_cast<const char*>(authKey.data()), authKey.size());
        regData.hostname = hostname_.empty() ? "artemis-switch" : hostname_;
        regData.capabilityVersion = capVer;

        const std::string regJson = encodeRegisterRequest(regData);
        if (!regJson.empty()) {
            registerStreamId_ = streamId;
            LOG_SESSION_INFO("sending /machine/register on stream " +
                             std::to_string(streamId) + " hostname=" +
                             regData.hostname);
            const auto headers =
                buildHttp2PostHeaders(streamId, host_, "/machine/register");
            if (!noise_->frame(headers, &framed, error) ||
                !transport_->write(framed, error)) {
                transport_->close();
                resetTransport();
                return false;
            }

            const auto dataFrame = buildHttp2DataFrame(
                streamId,
                std::span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>(regJson.data()),
                    regJson.size()),
                true);
            if (!noise_->frame(dataFrame, &framed, error) ||
                !transport_->write(framed, error)) {
                transport_->close();
                resetTransport();
                return false;
            }

            // Wait for registration response on stream 1 before opening map stream
            bool regComplete = false;
            while (!regComplete) {
                auto h2Frame = http2Decoder_.take();
                if (h2Frame) {
                    traceFrame(*h2Frame);
                    if (!creditData(*h2Frame, error)) {
                        transport_->close();
                        resetTransport();
                        return false;
                    }
                    if (h2Frame->type == 0x04) { // SETTINGS
                        if ((h2Frame->flags & 0x01) == 0) {
                            const auto ack = buildHttp2SettingsAck();
                            std::vector<std::uint8_t> framedAck;
                            if (noise_->frame(ack, &framedAck, error))
                                transport_->write(framedAck, error);
                        }
                        continue;
                    }
                    if (h2Frame->type == 0x00 && h2Frame->streamId == streamId) { // DATA
                        LOG_SESSION_INFO("Registration response received (" +
                                         std::to_string(h2Frame->payload.size()) +
                                         " bytes, HTTP " +
                                         std::to_string(registerHttpStatus_) + ").");
                        logRegisterResponse(h2Frame->payload);
                        regComplete = true;
                        break;
                    }
                    continue;
                }
                std::string chunk;
                if (!readNextNoiseRecord(&chunk, error)) {
                    transport_->close();
                    resetTransport();
                    return false;
                }
                http2Decoder_.append(std::span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t*>(chunk.data()),
                    chunk.size()));
            }
            streamId += 2;
        }
    }

    // 2. Start the streaming netmap request to /machine/map
    MapRequestData mapData;
    mapData.nodePublic = nodePublic;
    mapData.discoPublic = discoPublic;
    mapData.stream = true;
    mapData.hostname = hostname_.empty() ? "artemis-switch" : hostname_;
    mapData.capabilityVersion = capVer;
    // Home region known from a previous session: advertise it up front so
    // peers can reach this node from the first netmap instead of waiting
    // for a follow-up Hostinfo update.
    mapData.preferredDerp = initialPreferredDerp_;

    const std::string mapJson = encodeMapRequest(mapData);
    if (!mapJson.empty()) {
        mapStreamId_ = streamId;
#if defined(__SWITCH__)
        {
            const auto sent = nlohmann::json::parse(mapJson, nullptr, false);
            const bool hasNetInfo = sent.is_object() &&
                                    sent.contains("Hostinfo") &&
                                    sent["Hostinfo"].is_object() &&
                                    sent["Hostinfo"].contains("NetInfo");
            const std::size_t endpointCount =
                sent.is_object() && sent.contains("Endpoints") &&
                        sent["Endpoints"].is_array()
                    ? sent["Endpoints"].size()
                    : 0;
            LOG_SESSION_INFO("sending /machine/map on stream " +
                             std::to_string(streamId) +
                             " stream=true endpoints=" +
                             std::to_string(endpointCount) + " NetInfo=" +
                             (hasNetInfo ? "present" : "absent"));
            if (hasNetInfo)
                LOG_SESSION_INFO("MapRequest advertises home DERP region " +
                                 std::to_string(initialPreferredDerp_) +
                                 " from the previous session");
            else
                LOG_SESSION_INFO(
                    "MapRequest has no home DERP region yet (first session "
                    "on this device); one is advertised right after the "
                    "first netmap");
        }
#endif
        const auto headers =
            buildHttp2PostHeaders(streamId, host_, "/machine/map");
        if (!noise_->frame(headers, &framed, error) ||
            !transport_->write(framed, error)) {
            transport_->close();
            resetTransport();
            return false;
        }

        const auto dataFrame = buildHttp2DataFrame(
            streamId,
            std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(mapJson.data()),
                mapJson.size()),
            true); // endStream = true so server generates MapResponse
        if (!noise_->frame(dataFrame, &framed, error) ||
            !transport_->write(framed, error)) {
            transport_->close();
            resetTransport();
            return false;
        }
    }

    nextStreamId_ = streamId + 2;
    LOG_SESSION_INFO("Control session connected and HTTP/2 stream established.");
    ready_ = true;
    return true;
}

bool TailscaleControlSession::sendHostinfoUpdate(int preferredDerp,
                                                 std::string* error) {
    if (recordReader_)
        return true; // test seam: no live control plane
    if (!ready_ || !transport_ || !noise_) {
        if (error) *error = "control session is not connected";
        return false;
    }
    // Same shape tailscaled uses for endpoint/Hostinfo refreshes: a
    // non-streaming map request with OmitPeers on its own HTTP/2 stream. The
    // long-lived streaming map request on mapStreamId_ is unaffected.
    MapRequestData update;
    update.nodePublic = nodePublic_;
    update.discoPublic = discoPublic_;
    update.stream = false;
    update.omitPeers = true;
    update.hostname = hostname_.empty() ? "artemis-switch" : hostname_;
    update.preferredDerp = preferredDerp;
    update.capabilityVersion = capabilityVersion_;
    const std::string json = encodeMapRequest(update);
    if (json.empty()) {
        if (error) *error = "cannot encode Hostinfo update";
        return false;
    }
    std::lock_guard writeLock(writeMutex_);
    const std::uint32_t streamId = nextStreamId_;
    nextStreamId_ += 2;
    std::vector<std::uint8_t> framed;
    const auto headers = buildHttp2PostHeaders(streamId, host_, "/machine/map");
    if (!noise_->frame(headers, &framed, error) ||
        !transport_->write(framed, error))
        return false;
    const auto dataFrame = buildHttp2DataFrame(
        streamId,
        std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(json.data()), json.size()),
        true);
    if (!noise_->frame(dataFrame, &framed, error) ||
        !transport_->write(framed, error))
        return false;
    LOG_SESSION_INFO("sent Hostinfo update on stream " +
                     std::to_string(streamId) +
                     ": NetInfo.PreferredDERP=" + std::to_string(preferredDerp));
    return true;
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
                                   std::string* localAddress,
                                   std::optional<std::vector<DerpRegion>>* derpMap,
                                   std::string* error) {
    if (!ready_ || (!recordReader_ && !transport_) || !delta || !fullPeers ||
        !localAddress || !derpMap) {
        if (error) *error = "Tailscale control session is not connected";
        return false;
    }
    if (fullPeers->has_value()) fullPeers->reset();
    derpMap->reset();
    delta->changed.clear();
    delta->removedStableIds.clear();
    delta->onlineChanges.clear();
    localAddress->clear();

    std::string record;
    if (recordReader_) {
        if (!recordReader_(&record, error)) return false;
    } else {
        for (;;) {
            auto h2Frame = http2Decoder_.take();
            if (h2Frame) {
                traceFrame(*h2Frame);
                if (!creditData(*h2Frame, error))
                    return false;
                if (h2Frame->type == 0x04) { // SETTINGS
                    if ((h2Frame->flags & 0x01) == 0) { // Not ACK
                        const auto ack = buildHttp2SettingsAck();
                        std::vector<std::uint8_t> framedAck;
                        std::lock_guard writeLock(writeMutex_);
                        if (noise_->frame(ack, &framedAck, error))
                            transport_->write(framedAck, error);
                    }
                    continue;
                }
                if (h2Frame->type == 0x06) { // PING
                    if ((h2Frame->flags & 0x01) == 0) { // Not ACK
                        const auto pong = buildHttp2PingAck(h2Frame->payload);
                        std::vector<std::uint8_t> framedPong;
                        std::lock_guard writeLock(writeMutex_);
                        if (noise_->frame(pong, &framedPong, error))
                            transport_->write(framedPong, error);
                    }
                    continue;
                }
                if (h2Frame->type == 0x00) { // DATA
                    // Only the streaming map request carries netmap frames;
                    // replies to Hostinfo updates on other streams are
                    // discarded so they cannot corrupt the frame decoder.
                    if (mapStreamId_ != 0 && h2Frame->streamId != mapStreamId_) {
                        if (!h2Frame->payload.empty())
                            LOG_SESSION_INFO(
                                "discarded " +
                                std::to_string(h2Frame->payload.size()) +
                                " bytes on control stream " +
                                std::to_string(h2Frame->streamId));
                        continue;
                    }
                    if (!mapFrameDecoder_.append(h2Frame->payload, error))
                        return false;
                    auto frame = mapFrameDecoder_.take(error);
                    if (frame) {
                        record = std::move(*frame);
                        break;
                    }
                }
                continue;
            }
            std::string chunk;
            if (!readNextNoiseRecord(&chunk, error)) return false;
            http2Decoder_.append(std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(chunk.data()),
                chunk.size()));
        }
    }

    LOG_SESSION_INFO("Processing netmap JSON (" + std::to_string(record.size()) + " bytes): " + record.substr(0, std::min<size_t>(record.size(), 80)));
#if defined(__SWITCH__)
    // Top-level keys only (no values): reveals schema drift between control
    // implementations without dumping peer keys into the log.
    {
        const auto keyed =
            nlohmann::json::parse(record, nullptr, false);
        if (keyed.is_object()) {
            std::string keys;
            for (auto it = keyed.begin(); it != keyed.end(); ++it) {
                if (!keys.empty())
                    keys += ",";
                keys += it.key();
                if (keys.size() > 256) {
                    keys += ",...";
                    break;
                }
            }
            LOG_SESSION_INFO("netmap keys: [" + keys + "]");
            logNetmapDiagnostics(keyed, nodePublic_);
        }
    }
#endif
    auto update = mapCodec_.decode(record, error);
    if (!update) {
        LOG_SESSION_ERROR("MapCodec decode failed: " + (error ? *error : ""));
        return false;
    }
    if (!update->localAddress.empty()) {
        LOG_SESSION_INFO("Extracted local IP from netmap: " + update->localAddress);
    }
    if (update->keepAlive) return true;
    if (update->fullPeers) *fullPeers = std::move(*update->fullPeers);
    if (!update->localAddress.empty()) *localAddress = update->localAddress;
    if (update->derpMap) *derpMap = std::move(*update->derpMap);
    delta->changed = std::move(update->delta.changed);
    delta->removedStableIds = std::move(update->delta.removedStableIds);
    delta->onlineChanges = std::move(update->delta.onlineChanges);
    return true;
}

void TailscaleControlSession::resetTransport() noexcept {
    // Destroy outside the lock: a TLS teardown can take a moment and
    // interrupt() must not wait on it.
    std::unique_ptr<ITransport> old;
    {
        std::lock_guard lock(transportMutex_);
        old = std::move(transport_);
    }
    old.reset();
}

void TailscaleControlSession::interrupt() noexcept {
    std::lock_guard lock(transportMutex_);
    if (transport_) transport_->interrupt();
}

void TailscaleControlSession::close() noexcept {
    {
        std::lock_guard lock(transportMutex_);
        if (transport_) transport_->close();
    }
    resetTransport();
    noise_.reset();
    plaintextQueue_.clear();
    ready_ = false;
    recordReader_ = nullptr;
}

} // namespace artemis::tailscale
