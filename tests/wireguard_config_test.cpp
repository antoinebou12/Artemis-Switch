#include "../app/src/vpn/WireGuardConfig.hpp"

#include <cassert>
#include <string>

int main() {
    const char* conf = R"CONF(
[Interface]
PrivateKey = AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEE=
Address = 100.64.0.2/32
DNS = 1.1.1.1

[Peer]
PublicKey = BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB=
Endpoint = vpn.example.com:51820
AllowedIPs = 0.0.0.0/0
PersistentKeepalive = 25
)CONF";

    const auto parsed = parse_wireguard_conf(conf);
    assert(parsed.valid());
    assert(parsed.address == "100.64.0.2/32");
    assert(parsed.dns == "1.1.1.1");
    assert(parsed.peers.size() == 1);
    assert(parsed.peers[0].endpoint == "vpn.example.com:51820");
    assert(parsed.peers[0].persistentKeepalive == 25);

    const auto bad = parse_wireguard_conf("[Interface]\nAddress = 1.2.3.4/32\n");
    assert(!bad.valid());

    return 0;
}
