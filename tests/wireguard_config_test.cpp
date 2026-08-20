#include "../app/src/vpn/WireGuardConfig.hpp"

#include <cassert>
#include <string>

using Problem = WireGuardConfig::Problem;

namespace {

// Well-formed 44-char base64 keys (43 payload chars + '=').
const char* kKeyA = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEE=";
const char* kKeyB = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB=";
const char* kKeyC = "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC=";

std::string conf(const std::string& interfaceExtra,
                 const std::string& peerExtra) {
    return std::string("[Interface]\n") + "PrivateKey = " + kKeyA + "\n" +
           "Address = 100.64.0.2/32\n" + interfaceExtra + "\n[Peer]\n" +
           "PublicKey = " + kKeyB + "\n" + peerExtra + "\n";
}

} // namespace

int main() {
    // --- Happy path, every supported field ---------------------------------
    const char* full = R"CONF(
[Interface]
PrivateKey = AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEE=
Address = 100.64.0.2/32
DNS = 1.1.1.1
ListenPort = 51820
MTU = 1420

[Peer]
PublicKey = BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB=
PresharedKey = CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC=
Endpoint = vpn.example.com:51820
AllowedIPs = 0.0.0.0/0
PersistentKeepalive = 25
)CONF";

    const auto parsed = parse_wireguard_conf(full);
    assert(parsed.validate() == Problem::None);
    assert(parsed.valid());
    assert(parsed.address == "100.64.0.2/32");
    assert(parsed.dns == "1.1.1.1");
    assert(parsed.listenPort == 51820);
    assert(parsed.mtu == 1420);
    assert(parsed.peers.size() == 1);
    assert(parsed.peers[0].endpoint == "vpn.example.com:51820");
    assert(parsed.peers[0].persistentKeepalive == 25);
    // PresharedKey used to be dropped entirely, so a conf that required one
    // validated "OK" without it.
    assert(parsed.peers[0].presharedKey == kKeyC);
    assert(parsed.peers[0].allowedIps == "0.0.0.0/0");

    // --- Section headers ---------------------------------------------------
    // wg-quick tolerates case and trailing whitespace; we used to require an
    // exact "[Interface]" / "[Peer]" match.
    const auto loose = parse_wireguard_conf(
        std::string("[interface]  \nPrivateKey = ") + kKeyA +
        "\nAddress = 10.0.0.2/32\n\n  [PEER]\t\nPublicKey = " + kKeyB + "\n");
    assert(loose.validate() == Problem::None);

    // An unknown section must not have its keys attributed to the previous one.
    const auto unknownSection = parse_wireguard_conf(
        std::string("[Interface]\nPrivateKey = ") + kKeyA +
        "\nAddress = 10.0.0.2/32\n[Peer]\nPublicKey = " + kKeyB +
        "\n[Something]\nEndpoint = bogus\n");
    assert(unknownSection.validate() == Problem::None);
    assert(unknownSection.peers[0].endpoint.empty());

    // Both '#' and ';' introduce comments.
    const auto commented = parse_wireguard_conf(
        std::string("[Interface]\nPrivateKey = ") + kKeyA +
        " ; my key\nAddress = 10.0.0.2/32 # home\n[Peer]\nPublicKey = " + kKeyB +
        "\n");
    assert(commented.validate() == Problem::None);
    assert(commented.privateKey == kKeyA);

    // --- Missing pieces ----------------------------------------------------
    assert(parse_wireguard_conf("[Interface]\nAddress = 1.2.3.4/32\n")
               .validate() == Problem::MissingPrivateKey);
    assert(!parse_wireguard_conf("[Interface]\nAddress = 1.2.3.4/32\n").valid());

    assert(parse_wireguard_conf(std::string("[Interface]\nPrivateKey = ") + kKeyA + "\n")
               .validate() == Problem::MissingAddress);

    assert(parse_wireguard_conf(std::string("[Interface]\nPrivateKey = ") + kKeyA +
                                "\nAddress = 10.0.0.2/32\n")
               .validate() == Problem::NoPeers);

    // A [Peer] with no PublicKey is dropped, so it reads as "no peers".
    assert(parse_wireguard_conf(std::string("[Interface]\nPrivateKey = ") + kKeyA +
                                "\nAddress = 10.0.0.2/32\n[Peer]\nEndpoint = a:1\n")
               .validate() == Problem::NoPeers);

    // --- Malformed keys ----------------------------------------------------
    assert(parse_wireguard_conf("[Interface]\nPrivateKey = short\nAddress = 10.0.0.2/32\n")
               .validate() == Problem::MalformedPrivateKey);
    // Right length, but '!' is not in the base64 alphabet.
    assert(parse_wireguard_conf(
               "[Interface]\nPrivateKey = AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA!EE=\n"
               "Address = 10.0.0.2/32\n")
               .validate() == Problem::MalformedPrivateKey);
    // Missing the '=' padding.
    assert(parse_wireguard_conf(
               "[Interface]\nPrivateKey = AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEEA\n"
               "Address = 10.0.0.2/32\n")
               .validate() == Problem::MalformedPrivateKey);

    assert(conf("", std::string("PublicKey = nope")).size() > 0);
    {
        const auto badPeer = parse_wireguard_conf(
            std::string("[Interface]\nPrivateKey = ") + kKeyA +
            "\nAddress = 10.0.0.2/32\n[Peer]\nPublicKey = nope\n");
        assert(badPeer.validate() == Problem::MalformedPeerPublicKey);
    }
    {
        const auto badPsk = parse_wireguard_conf(
            conf("", std::string("PresharedKey = tooshort")));
        assert(badPsk.validate() == Problem::MalformedPeerPresharedKey);
    }

    // --- Addresses ---------------------------------------------------------
    assert(parse_wireguard_conf(conf("", "")).validate() == Problem::None);
    for (const char* bad : {"10.0.0", "10.0.0.256", "10.0.0.2/33", "abc",
                            "10.0.0.2/", "10.0.0..2"}) {
        const auto c = parse_wireguard_conf(
            std::string("[Interface]\nPrivateKey = ") + kKeyA + "\nAddress = " +
            bad + "\n[Peer]\nPublicKey = " + kKeyB + "\n");
        assert(c.validate() == Problem::MalformedAddress);
    }
    // Bare IPv4, IPv6, and comma-separated lists are all legal.
    for (const char* good : {"10.0.0.2", "10.0.0.2/32", "fd00::2/128",
                             "10.0.0.2/32, fd00::2/128"}) {
        const auto c = parse_wireguard_conf(
            std::string("[Interface]\nPrivateKey = ") + kKeyA + "\nAddress = " +
            good + "\n[Peer]\nPublicKey = " + kKeyB + "\n");
        assert(c.validate() == Problem::None);
    }

    // --- Endpoints ---------------------------------------------------------
    for (const char* bad : {"vpn.example.com", "vpn.example.com:0",
                            "vpn.example.com:70000", "vpn.example.com:abc",
                            "vpn.example.com:", ":51820"}) {
        const auto c = parse_wireguard_conf(
            conf("", std::string("Endpoint = ") + bad));
        assert(c.validate() == Problem::MalformedEndpoint);
    }
    assert(parse_wireguard_conf(conf("", "Endpoint = vpn.example.com:1"))
               .validate() == Problem::None);
    assert(parse_wireguard_conf(conf("", "Endpoint = vpn.example.com:65535"))
               .validate() == Problem::None);

    // --- Numeric fields ----------------------------------------------------
    // atoi silently turned these into 0; they must be reported instead.
    assert(parse_wireguard_conf(conf("ListenPort = abc", "")).validate() ==
           Problem::InvalidListenPort);
    assert(parse_wireguard_conf(conf("ListenPort = 70000", "")).validate() ==
           Problem::InvalidListenPort);
    assert(parse_wireguard_conf(conf("MTU = wat", "")).validate() ==
           Problem::InvalidMtu);
    assert(parse_wireguard_conf(conf("MTU = 42", "")).validate() ==
           Problem::InvalidMtu);
    assert(parse_wireguard_conf(conf("MTU = 1420", "")).validate() == Problem::None);
    assert(parse_wireguard_conf(conf("", "PersistentKeepalive = nope"))
               .validate() == Problem::InvalidKeepalive);
    // Absent numeric fields stay 0 and must not trip validation.
    assert(parse_wireguard_conf(conf("", "")).listenPort == 0);
    assert(parse_wireguard_conf(conf("", "")).mtu == 0);

    // --- Multiple peers ----------------------------------------------------
    {
        const auto multi = parse_wireguard_conf(
            std::string("[Interface]\nPrivateKey = ") + kKeyA +
            "\nAddress = 10.0.0.2/32\n[Peer]\nPublicKey = " + kKeyB +
            "\n[Peer]\nPublicKey = " + kKeyC + "\n");
        assert(multi.validate() == Problem::None);
        assert(multi.peers.size() == 2);
        // A bad second peer must not be masked by a good first one.
        const auto multiBad = parse_wireguard_conf(
            std::string("[Interface]\nPrivateKey = ") + kKeyA +
            "\nAddress = 10.0.0.2/32\n[Peer]\nPublicKey = " + kKeyB +
            "\n[Peer]\nPublicKey = " + kKeyC + "\nEndpoint = host:0\n");
        assert(multiBad.validate() == Problem::MalformedEndpoint);
    }

    // --- Problem keys are distinct and translatable ------------------------
    {
        const Problem all[] = {
            Problem::None, Problem::MissingPrivateKey, Problem::MalformedPrivateKey,
            Problem::MissingAddress, Problem::MalformedAddress, Problem::NoPeers,
            Problem::MissingPeerPublicKey, Problem::MalformedPeerPublicKey,
            Problem::MalformedPeerPresharedKey, Problem::MalformedEndpoint,
            Problem::InvalidListenPort, Problem::InvalidKeepalive,
            Problem::InvalidMtu,
        };
        for (std::size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i) {
            const std::string a = wireguard_problem_i18n_key(all[i]);
            assert(a.rfind("artemis/settings/wireguard_problem_", 0) == 0);
            for (std::size_t j = i + 1; j < sizeof(all) / sizeof(all[0]); ++j) {
                assert(a != wireguard_problem_i18n_key(all[j]));
            }
        }
    }

    // --- Secret scrubbing --------------------------------------------------
    {
        std::string secret = kKeyA;
        wireguard_scrub(secret);
        assert(secret.empty());
    }

    return 0;
}
