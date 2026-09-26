#include "TailscaleProvider.hpp"

#include "../IRemoteAccessProvider.hpp"
#include "../../utils/Settings.hpp"
#include "../../vpn/VpnFileLogger.hpp"
#include "../tailscale/TailscaleControlSession.hpp"
#include "../tailscale/TailscaleControlKey.hpp"
#include "../tailscale/TailscaleAuthKeyFile.hpp"
#include "TailscaleCompatibility.hpp"
#include "../tailscale/TailscaleCore.hpp"
#include "../tailscale/TailscaleStateStore.hpp"
#include "../tailscale/TailscaleTypes.hpp"
#include "../tailscale/TailscaleWgxRoute.hpp"
#include "../tailscale/TailscaleRandom.hpp"

#include <filesystem>
#include <string_view>
#include <unordered_set>

#include <cstdint>
#include <cstring>

using namespace artemis::tailscale;

namespace {

void wipe(std::string& value) noexcept {
    std::fill(value.begin(), value.end(), '\0');
    value.clear();
    value.shrink_to_fit();
}

void logTs(VpnFileLogger::Severity severity, std::string_view message) {
    VpnFileLogger::append(Settings::instance().working_dir() + "/vpn.log",
                          "TS", severity, message);
}

// The log sanitizer blanks any whole line mentioning an "auth key", which hid
// every login-file diagnostic in earlier logs. These messages never contain
// the key itself, so reword the phrase before logging.
std::string logSafe(std::string text) {
    for (const std::string_view needle : {"auth key", "Auth key", "AUTH KEY"}) {
        for (auto at = text.find(needle); at != std::string::npos;
             at = text.find(needle, at)) {
            text.replace(at, needle.size(), "login-key");
            at += 9;
        }
    }
    return text;
}

// resolveRoute runs for every endpoint of every host on each refresh; log a
// given address/result pair once so the log is not flooded. Guarded by the
// provider mutex held in resolveRoute.
std::unordered_set<std::string> g_loggedResolves;

bool decodeControlKey(const std::string& encoded,
                      artemis::tailscale::Key32& key) {
    const auto parsed = artemis::tailscale::decodeTypedKey(encoded, "mkey:");
    if (!parsed)
        return false;
    key = *parsed;
    return
           !std::all_of(key.begin(), key.end(),
                        [](std::uint8_t byte) { return byte == 0; });
}

bool generateIdentity(artemis::tailscale::Identity& identity,
                      std::string* error) {
    // Kernel CSPRNG. A fresh std::random_device per key replays the same
    // sequence on Switch and produced three identical private keys.
    artemis::tailscale::secureRandomBytes(identity.machinePrivate);
    artemis::tailscale::secureRandomBytes(identity.nodePrivate);
    artemis::tailscale::secureRandomBytes(identity.discoPrivate);
    if (artemis::tailscale::isWeakIdentity(identity)) {
        if (error)
            *error = "random source returned repeated key material";
        return false;
    }
    return true;
}

} // namespace

TailscaleProvider::~TailscaleProvider() { stop(); }

bool TailscaleProvider::available() const {
#if defined(__SWITCH__) && defined(ENABLE_TAILSCALE)
    // A compiled experimental backend is not the same as an accepted live
    // protocol implementation. Keep it unavailable until the reviewed
    // compatibility manifest records a capability that passed the dedicated
    // SaaS/DERP/Moonlight gate.
    return compat::kAcceptedCapabilityVersion > 0;
#else
    return false;
#endif
}

bool TailscaleProvider::start() {
    std::lock_guard lock(mutex_);
#if !defined(__SWITCH__) || !defined(ENABLE_TAILSCALE)
    status_ = "Unavailable";
    lastError_ = "Tailscale is unavailable in this build";
    return false;
#else
    if (compat::kAcceptedCapabilityVersion <= 0) {
        status_ = "Unavailable (live gate not passed)";
        lastError_ =
            "Tailscale capability and DERP streaming acceptance are pending";
        logTs(VpnFileLogger::Severity::Warning, lastError_);
        wipe(authKey_);
        wipe(passphrase_);
        return false;
    }
    if (Settings::instance().remote_access_provider() !=
        RemoteAccessProviderId::Tailscale) {
        status_ = "Stopped";
        lastError_ = "Tailscale is not the selected provider";
        return false;
    }

    if (authKey_.empty()) {
        auto loaded = loadAuthKeyFile(
            Settings::instance().working_dir(),
            Settings::instance().tailscale_auth_key_path());
        if (loaded.found()) {
            authKey_ = std::move(loaded.key);
            logTs(VpnFileLogger::Severity::Info,
                  "one-off login-key loaded from " + loaded.path);
        } else {
            status_ = "Needs authentication";
            lastError_ = loaded.error;
            logTs(VpnFileLogger::Severity::Warning,
                  logSafe(lastError_) + " (searched the configured path and "
                  "the defaults under " + Settings::instance().working_dir() +
                  "/tailscale/)");
            return false;
        }
    }
    logTs(VpnFileLogger::Severity::Info,
          "provider start: control host=" +
              Settings::instance().tailscale_control_host() + ":" +
              std::to_string(Settings::instance().tailscale_control_port()) +
              " hostname=" + Settings::instance().tailscale_hostname() +
              " control key=" +
              (Settings::instance().tailscale_control_public_key().empty()
                   ? "fetch from server"
                   : "configured"));

    const std::string host = Settings::instance().tailscale_control_host();
    const std::string keyText = Settings::instance().tailscale_control_public_key();
    if (host.empty()) {
        status_ = "Needs configuration";
        lastError_ = "Tailscale control host is not configured";
        logTs(VpnFileLogger::Severity::Error, lastError_);
        wipe(authKey_);
        wipe(passphrase_);
        return false;
    }
    Key32 controlKey{};
    if (keyText.empty()) {
        SwitchTlsTransport keyTransport;
        auto fetched = fetchControlPublicKey(
            keyTransport, host, Settings::instance().tailscale_control_port(),
            compat::kAcceptedCapabilityVersion, &lastError_);
        if (!fetched) {
            status_ = "Control key unavailable";
            logTs(VpnFileLogger::Severity::Error, lastError_);
            wipe(authKey_);
            wipe(passphrase_);
            return false;
        }
        controlKey = *fetched;
    } else if (!decodeControlKey(keyText, controlKey)) {
        status_ = "Needs configuration";
        lastError_ = "Tailscale control public key must be mkey: plus 64 lowercase hex digits";
        logTs(VpnFileLogger::Severity::Error, lastError_);
        wipe(authKey_);
        wipe(passphrase_);
        return false;
    }

    const auto statePath =
        std::filesystem::path(Settings::instance().working_dir()) /
        "tailscale" / "state.bin";

    core_ = std::make_shared<TailscaleCore>(
        statePath,
        std::make_unique<TailscaleControlSession>(
            [] { return std::make_unique<SwitchTlsTransport>(); }, host,
            Settings::instance().tailscale_control_port(), controlKey,
            Settings::instance().tailscale_hostname()),
        std::make_unique<TailscaleWgxRoute>(), generateIdentity);

    SecureBytes authKey(authKey_);
    SecureBytes passphrase(passphrase_);
    const bool started =
        core_->start(std::move(authKey), std::move(passphrase));
    status_ = started ? "Starting" : "Failed";
    lastError_.clear();
    logTs(VpnFileLogger::Severity::Info,
          "control engine started for " + host +
              " (DERP-relay data path)");
    wipe(authKey_);
    wipe(passphrase_);
    return started;
#endif
}

void TailscaleProvider::stop() {
    std::lock_guard lock(mutex_);
#if defined(__SWITCH__) && defined(ENABLE_TAILSCALE)
    if (core_) {
        core_->stop();
        core_.reset();
        logTs(VpnFileLogger::Severity::Info, "provider stopped");
    }
#endif
    status_ = "Stopped";
    lastError_.clear();
    wipe(authKey_);
    wipe(passphrase_);
}

std::string TailscaleProvider::status() const {
    std::lock_guard lock(mutex_);
#if !defined(__SWITCH__) || !defined(ENABLE_TAILSCALE)
    return status_;
#else
    if (!core_) {
        if (compat::kAcceptedCapabilityVersion <= 0)
            return "Unavailable (live gate not passed)";
        return status_;
    }
    const auto snapshot = core_->snapshot();
    switch (snapshot.state) {
    case Snapshot::State::Stopped:
        return "Stopped";
    case Snapshot::State::Starting:
        return "Starting";
    case Snapshot::State::NeedsAuthentication:
        return "Needs authentication";
    case Snapshot::State::ConnectingControl:
        return "Connecting to control";
    case Snapshot::State::ConnectedControl:
        return "Control connected";
    case Snapshot::State::Ready:
        // The control plane is up and peers are known, but the encrypted
        // packet path is still gated closed until a peer session is usable.
        return "Ready";
    case Snapshot::State::Error:
        return snapshot.lastError.empty() ? "Error" : snapshot.lastError;
    }
    return status_;
#endif
}

std::string TailscaleProvider::lastError() const {
    std::lock_guard lock(mutex_);
#if !defined(__SWITCH__) || !defined(ENABLE_TAILSCALE)
    return lastError_;
#else
    if (core_) {
        const auto snapshot = core_->snapshot();
        if (snapshot.state == Snapshot::State::Error)
            return snapshot.lastError;
    }
    return lastError_;
#endif
}

std::string TailscaleProvider::localAddress() const {
    std::lock_guard lock(mutex_);
#if !defined(__SWITCH__) || !defined(ENABLE_TAILSCALE)
    return {};
#else
    return core_ ? core_->snapshot().localAddress : std::string{};
#endif
}

std::vector<RemoteAccessPeer> TailscaleProvider::peers() const {
    std::vector<RemoteAccessPeer> result;
    std::lock_guard lock(mutex_);
#if !defined(__SWITCH__) || !defined(ENABLE_TAILSCALE)
    return result;
#else
    if (!core_)
        return result;
    const auto snapshot = core_->snapshot();
    result.reserve(snapshot.peers.size());
    for (const auto& peer : snapshot.peers) {
        RemoteAccessPeer entry;
        entry.providerId = "tailscale";
        entry.peerId = peer.stableId;
        entry.name = peer.hostname.empty() ? peer.stableId : peer.hostname;
        entry.address = peer.addresses.empty() ? std::string{} : peer.addresses[0];
        entry.online = peer.online;
        entry.metadata = peer.homeDerp > 0
                             ? "derp-" + std::to_string(peer.homeDerp)
                             : std::string{};
        result.push_back(std::move(entry));
    }
    return result;
#endif
}

std::optional<RemoteRouteTarget>
TailscaleProvider::resolveRoute(std::string_view address) const {
    std::lock_guard lock(mutex_);
#if !defined(__SWITCH__) || !defined(ENABLE_TAILSCALE)
    return std::nullopt;
#else
    auto route = core_ ? core_->resolveRoute(address) : std::nullopt;
    std::string entry = std::string(address) + " -> ";
    if (!core_)
        entry += "provider not running";
    else if (route)
        entry += "tailnet peer " + route->peerId;
    else
        entry += "not a tailnet peer";
    if (g_loggedResolves.size() < 128 && g_loggedResolves.insert(entry).second) {
        std::string message = "resolve " + entry;
        if (core_ && !route)
            message += " (dialed directly, not through Tailscale; known peers=" +
                       std::to_string(core_->snapshot().peers.size()) + ")";
        logTs(VpnFileLogger::Severity::Info, message);
    }
    return route;
#endif
}

bool TailscaleProvider::activateRoute(const RemoteRouteTarget& target) {
    std::lock_guard lock(mutex_);
#if !defined(__SWITCH__) || !defined(ENABLE_TAILSCALE)
    (void)target;
    return false;
#else
    if (!core_)
        return false;
    return core_->activateRoute(target);
#endif
}

bool TailscaleProvider::prepareRouteForStreaming(
    const RemoteRouteTarget& target) {
    std::lock_guard lock(mutex_);
#if !defined(__SWITCH__) || !defined(ENABLE_TAILSCALE)
    (void)target;
    return false;
#else
    if (!core_)
        return false;
    return core_->prepareRouteForStreaming(target);
#endif
}

void TailscaleProvider::deactivateRoute(const RemoteRouteTarget& target) {
    std::lock_guard lock(mutex_);
#if defined(__SWITCH__) && defined(ENABLE_TAILSCALE)
    if (core_)
        core_->deactivateRoute(target);
#else
    (void)target;
#endif
}

RemotePathInfo TailscaleProvider::pathInfo(std::string_view peerId) const {
    std::lock_guard lock(mutex_);
#if !defined(__SWITCH__) || !defined(ENABLE_TAILSCALE)
    (void)peerId;
    return RemotePathInfo{};
#else
    return core_ ? core_->pathInfo(peerId) : RemotePathInfo{};
#endif
}

void TailscaleProvider::setOneOffAuthKey(std::string key) {
    std::lock_guard lock(mutex_);
    wipe(authKey_);
    authKey_ = std::move(key);
}

void TailscaleProvider::setLaunchPassphrase(std::string passphrase) {
    std::lock_guard lock(mutex_);
    wipe(passphrase_);
    passphrase_ = std::move(passphrase);
}

void TailscaleProvider::clearTransientSecrets() noexcept {
    std::lock_guard lock(mutex_);
    wipe(authKey_);
    wipe(passphrase_);
}
