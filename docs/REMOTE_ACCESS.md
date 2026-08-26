# Remote Access (NetBird + WireGuard)

Artemis Switch can stream from a host that is not on the local network, over
either NetBird's mesh VPN or a plain WireGuard tunnel. Both are **compiled into
every Switch build**. "Off" in the UI means no tunnel is currently connected —
it never means the feature was left out of the build.

## Architecture

```
                  Artemis-Switch
                        |
              RemoteAccessManager          <- polled once per frame
                        |
          +-------------+-------------+
          |                           |
 WireGuardProvider              NetBirdProvider
          |                           |
   wg0.conf parsing            setup-key login
          |                     peer sync + relay
          |                           |
          +-------------+-------------+
                        |
                  shared wg-nx          <- one implementation, not two
                        |
                      lwIP
                        |
              local TCP + UDP proxies
                        |
                    127.0.0.1
                        |
                  Moonlight stack
                        |
                 Sunshine / Apollo
```

Moonlight itself always connects to `127.0.0.1`. The proxy carries that traffic
through lwIP and WireGuard to the remote peer. The peer's real mesh address
stays as host identity — it is never overwritten with `127.0.0.1`.

## Build

Both providers are on by default for Switch:

```bash
cmake -B build/switch -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON
cmake --build build/switch --target Moonlight.nro
```

No `-DENABLE_WIREGUARD=ON -DENABLE_NETBIRD=ON` needed — those are the defaults.
They are rejected outright on non-Switch platforms.

The build compiles `libnetbird.a` and `handle_full.o` from the pinned
`extern/netbird-switch` submodule automatically. A fresh clone needs only:

```bash
git submodule update --init --recursive
```

### Why the backend is staged before building

The vendored NetBird `Makefile` derives every path from `$(CURDIR)` and cannot
build from a tree whose path contains spaces (the default checkout lives under
"New project"). `cmake/NetBirdBackend.cmake` copies the sources into
`ARTEMIS_VPN_STAGE_ROOT` (default `/tmp/artemis-vpn-build`) and builds there.
Point that cache variable somewhere else if `/tmp` is unsuitable, but keep it
free of spaces.

### There is no silent stub

Earlier builds fell back to `wg_nx_stub.cpp` when no real backend was found.
That produced an NRO that looked fine and tunnelled nothing. Now a Switch build
without a real backend is a configure-time `FATAL_ERROR`.

`-DARTEMIS_ALLOW_VPN_STUB=ON` still selects the stub for desktop and unit-test
work. It prints a loud warning and must never be used for a release NRO. CI
sets it `OFF` explicitly and fails if the real symbols are missing from
`Moonlight.elf`.

### Link order matters

`handle_full.o` must precede every toolchain library. It overrides newlib's
file-descriptor management; without that ordering, concurrent socket I/O
against `close`/`shutdown` crashes in `_socketGetFd` / `_read_r` during proxy
teardown and peer switching. This is the same fix upstream needed after the
first 720p60 stream turned out to be crash-prone.

## Configuration

Both are configured in **Settings → Remote Access** and stored in
`settings.json`.

### NetBird

| Setting | Key | Notes |
| --- | --- | --- |
| Management server | `netbird_server` | Defaults to `https://api.netbird.io:443` |
| Setup key | `netbird_setup_key` | A credential — never logged |

Login goes through `netbird_init()`, which performs setup-key authentication,
peer sync, relay connection, WireGuard key derivation and handshake. Artemis
does not reimplement any part of that protocol.

### WireGuard

A standard `wg0.conf`:

```ini
[Interface]
PrivateKey = <base64>
Address = 10.70.0.2/32

[Peer]
PublicKey = <base64>
Endpoint = vpn.example.com:51820
AllowedIPs = 10.70.0.0/24
PersistentKeepalive = 25
```

Default location: `sdmc:/switch/Artemis-Switch/wg0.conf`.

> Keep credential files out of the repository. `netbird_config.json`,
> `netbird_switch_config.json`, `netbird.conf` and `wg0.conf` are gitignored.

## Verifying a build has the real backend

```bash
aarch64-none-elf-nm build/switch/Moonlight.elf | grep -E " T (netbird_init|wg_init)$"
```

CI runs this check over the full symbol list and fails the build if any are
missing.

At runtime, the Remote Access status row appends "Unavailable (stub build)"
whenever `wg_nx_is_real_backend()` is false, so a broken build cannot be
mistaken for a working one.

## Status

Verified by build:

- Both providers compile into the Switch NRO and link against the real backend.
- All real backend symbols are present; no stub object is linked.
- `ENABLE_WIREGUARD=ON` with `ENABLE_NETBIRD=OFF` fails configure as intended.

**Not yet verified on hardware.** The following still need a real Switch, a
NetBird account and a Sunshine/Apollo host, and should not be claimed as
working until they pass:

- NetBird: setup-key login, tunnel IP assignment, peer sync, peer
  auto-discovery, pairing, app list, launch, video/audio/input, host switching,
  Wi-Fi reconnect, HOME/resume, clean shutdown.
- WireGuard: handshake, TCP and UDP through the tunnel, pairing, launch,
  video/audio/control, reconnect, clean shutdown.
- LAN streaming unchanged while Remote Access is Off.
