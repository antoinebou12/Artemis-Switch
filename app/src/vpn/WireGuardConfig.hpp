#pragma once

#include <string>
#include <vector>

struct WireGuardPeerConfig {
    std::string publicKey;
    std::string presharedKey;
    // Parsed and validated, but not yet consumed: a real tunnel backend would
    // use endpoint/allowedIps/persistentKeepalive to open the UDP session and
    // decide what traffic to route. Validating them anyway means a bad config
    // is reported now rather than silently failing later.
    std::string endpoint;
    std::string allowedIps;
    int persistentKeepalive = 0;
};

struct WireGuardConfig {
    std::string privateKey;
    std::string address;
    // Same as above: parsed for completeness, consumed only by a real backend.
    std::string dns;
    int listenPort = 0;
    int mtu = 0;
    std::vector<WireGuardPeerConfig> peers;

    // Why validation failed, so the UI can say something specific instead of
    // "invalid config". Ordered roughly by how early the problem appears.
    enum class Problem {
        None,
        MissingPrivateKey,
        MalformedPrivateKey,
        MissingAddress,
        MalformedAddress,
        NoPeers,
        MissingPeerPublicKey,
        MalformedPeerPublicKey,
        MalformedPeerPresharedKey,
        MalformedEndpoint,
        InvalidListenPort,
        InvalidKeepalive,
        InvalidMtu,
    };

    [[nodiscard]] Problem validate() const;
    [[nodiscard]] bool valid() const { return validate() == Problem::None; }
};

// A stable, translatable identifier for a validation problem. Returns an i18n
// key under "artemis/settings/"; the caller resolves it.
const char* wireguard_problem_i18n_key(WireGuardConfig::Problem problem);

// Parse a standard WireGuard .conf (Interface + Peer sections).
WireGuardConfig parse_wireguard_conf(const std::string& text);
std::string load_text_file(const std::string& path);

// Overwrite a secret in place before it is dropped. Not a hard guarantee under
// an optimising compiler, but it keeps keys out of freed heap in practice.
void wireguard_scrub(std::string& secret);
