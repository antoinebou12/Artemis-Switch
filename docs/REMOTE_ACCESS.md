# Remote Access (NetBird + WireGuard)

Artemis Switch can stream from a host that is not on the local network, over
either NetBird's mesh VPN or a plain WireGuard tunnel. Both are **compiled into
every Switch build**. "Off" in the UI means no tunnel is currently connected —
it never means the feature was left out of the build.

## Architecture

```
                  Artemis-Switch
                        |
              RemoteAccessManager          <- WireGuard frame polling
                        |
          +-------------+-------------+
          |                           |
 WireGuardProvider              NetBirdProvider
          |                           |
   wg0.conf parsing            setup-key login
          |                     peer sync + relay
          |                    5 ms pump thread
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

When a session is routed through the tunnel, Moonlight connects to `127.0.0.1`
and the proxy carries that traffic through lwIP and WireGuard to the peer. A
LAN-reachable host is dialled directly and never touches the proxy — see
[How a remote stream is routed](#how-a-remote-stream-is-routed). Either way the
peer's real mesh address stays as host identity; it is never overwritten with
`127.0.0.1`.

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

After initialization, `NetBirdProvider` starts a persistent 5 ms pump before
probing peers. It services lwIP and WireGuard timers independently of the UI,
including while a blocking GameStream HTTP request is waiting on the localhost
proxy. `vpn.log` records a five-second watchdog and the number of pump cycles
that occurred during every peer probe.

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

## How a remote stream is routed

`GameStreamClient` walks the host's endpoints in priority order (LAN 0, manual
remote 1, NetBird 2). For each candidate, `artemis::remote::acquireRouteFor()`
checks whether the address is a peer from authenticated NetBird sync:

- **Not a peer** (LAN, direct WAN) — no lease, dial the address unchanged.
- **A peer** — start that peer's TCP proxy and dial `127.0.0.1` instead. The
  five UDP media relays are started only after the launch request succeeds, so
  discovery and pairing do not consume the stream-time socket/thread budget.

So LAN is always tried first and never detours through the tunnel. The lease
lives in `m_active_routes` for the whole session, because pairing, the app list,
launch and the stream all travel through the same proxy. Assigning over a key
releases the previous peer's route, which is how host switching works.

`vpn.log` records the authenticated peer count, each `47989` reachability probe,
the required TCP listeners (`47989`, `47984`, `48010`), the exact GameStream
handshake result, UDP-relay startup, and the peer whose route is released. A
"route active" line therefore no longer hides a missing HTTPS/RTSP listener or
a later HTTP timeout.

The host keeps its **real mesh address as identity**; only the transport is
loopback. (The reference integration stores `127.0.0.1` as the address, which
collapses every peer onto one cache key and needs a `host_key()` special case.)

### Why Moonlight needs adapting for a loopback proxy

Three things change once traffic goes through the proxy, all applied only when
the session is actually proxied:

| Adaptation | Why |
| --- | --- |
| `serverinfo` forced to plain HTTP | TLS to the host does not complete through the relay |
| Long HTTP timeout | The tunnel adds 100–300 ms per round trip; the LAN-tuned timeout expires first |
| `sessionUrl0` host rewritten to loopback | The host reports *its own* overlay address, and moonlight-common-c prefers that URL — without the rewrite RTSP bypasses the proxy and the stream dies right after pairing |

### Dependency patches

Submodules stay on their upstream pins; these apply to build-directory copies.

- **`borealis-bsd-session-pool.patch`** — `num_bsd_sessions` 12 → 16 (applet
  2 → 6). Every blocking bsd call holds a session for its full duration, and
  NetBird adds ~10 threads doing concurrent socket I/O on top of curl and
  Moonlight. 12 sits at the exhaustion edge, and an exhausted pool does not fail
  gracefully: `bsdSend` blocks while the caller holds the lwIP lock, deadlocking
  the app.
- **`moonlight-common-c-loopback-private.patch`** — treat 127.0.0.0/8 as
  private, so a proxied session is not classified as remote and the host does
  not withhold its local-streaming settings.

### Threading

`netbird_init()` is a network login and `netbird_shutdown()` joins ~10 worker
threads — both freeze the app if run on the UI thread. Connect and disconnect go
through `applyRemoteAccessSelectionAsync()`: worker thread, modal loading
dialog, result delivered back with `brls::sync`. Peer probing costs a round trip
each, so `peers()` is a cache read and `refreshPeers()` (which probes) only ever
runs on a worker. Every async continuation is guarded by a shared `alive` flag
cleared in the owning view's destructor.

## Status

Verified by build:

- Both providers compile into the Switch NRO and link against the real backend.
- All real backend symbols are present; no stub object is linked.
- `ENABLE_WIREGUARD=ON` with `ENABLE_NETBIRD=OFF` fails configure as intended.
- Both dependency patches apply and are present in the build copies.
- `acquireRouteFor()` has a caller in the streaming path, so the proxy is
  actually started (it previously was not — `activateRoute()` was dead code).

**Not yet verified on hardware.** The following still need a real Switch, a
NetBird account and a Sunshine/Apollo host, and should not be claimed as
working until they pass:

- NetBird: setup-key login, tunnel IP assignment, peer sync, peer
  auto-discovery, pairing, app list, launch, video/audio/input, host switching,
  Wi-Fi reconnect, HOME/resume, clean shutdown.
- WireGuard: handshake, TCP and UDP through the tunnel, pairing, launch,
  video/audio/control, reconnect, clean shutdown.
- LAN streaming unchanged while Remote Access is Off.
