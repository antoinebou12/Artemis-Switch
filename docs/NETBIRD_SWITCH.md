# NetBird remote access on Nintendo Switch

Artemis can use the `jmpangilinan/netbird-switch` userspace VPN to reach a Sunshine or Apollo host through NetBird while staying on Horizon OS.

The Switch has no TUN device, so this is an application-scoped VPN:

```text
Artemis / Moonlight
  -> 127.0.0.1 TCP + UDP proxy
  -> embedded lwIP
  -> wg-nx WireGuard
  -> NetBird relay (WebSocket + TLS)
  -> Sunshine / Apollo peer
```

## Current behavior

- NetBird is Switch-only and linked into Artemis.
- The dependency is pinned to commit `55d5b04fe7d666b4a2d2a324884caf6b0e926212`, which contains the disconnect/reconnect cleanup fixes.
- `handle_full.o` is linked before the normal libraries to avoid the devkitPro/newlib descriptor-table race documented by the NetBird Switch project.
- Opening **Add Host** also authenticates with NetBird and adds synchronized peers to the search list.
- A normal LAN/public host is not changed.
- When `gs_init()` receives an address that exactly matches a synchronized NetBird peer, Artemis starts the NetBird TCP and UDP proxies and transparently connects GameStream to `127.0.0.1`.
- Only one NetBird peer is routed at a time, matching `netbird-switch`.
- NetBird polling runs on one dedicated ~16 ms polling thread. This keeps lwIP/WireGuard timers alive without adding NetBird calls throughout the render loop.

## Configuration

Create `netbird.conf` in the same Artemis working directory as `settings.json`:

```ini
# NetBird management API
server=https://api.netbird.io:443

# Create a reusable setup key in NetBird and paste it here.
# Artemis never writes or logs this value.
setup_key=YOUR-SETUP-KEY
```

On Switch the fallback path, used only if the Artemis working directory has not been initialized yet, is:

```text
sdmc:/switch/Artemis-Switch/netbird.conf
```

The regular working-directory location is preferred.

## Build

Clone recursively:

```bash
git clone --recursive https://github.com/antoinebou12/Artemis-Switch.git
```

If the checkout already exists:

```bash
git submodule update --init --recursive extern/netbird-switch
```

The Switch CMake build invokes `extern/netbird-switch/build.sh` automatically. On Windows, configure/build from devkitPro MSYS2 so `bash`, devkitA64 and the required portlibs are available. The build command forces `TMP`, `TEMP` and `TMPDIR` to `/tmp`, matching the upstream Windows workaround.

Required devkitPro portlibs include mbedTLS, zlib and curl. Artemis continues using its Switch mbedTLS curl build.

## Discovery and streaming

1. Configure the same NetBird network on the Sunshine/Apollo PC.
2. Put `netbird.conf` beside the Artemis settings file.
3. Start Artemis and open **Add Host**.
4. NetBird peers are merged into the search results alongside normal LAN mDNS results.
5. Select the gaming PC and pair normally.
6. When Artemis connects to that peer, the GameStream address is routed through the local NetBird proxy automatically.
7. TCP setup traffic and Moonlight UDP video/audio/control traffic remain behind the same active peer route for the stream.

## Limitations inherited from netbird-switch

- Per-application VPN only. It does not provide a system-wide Horizon VPN.
- Relay transport only. Direct NetBird WebRTC/ICE P2P is not implemented by this library.
- One routed peer at a time.
- The embedded lwIP stack is single-threaded internally.
- A hardware Switch test is required for final validation of suspend/resume, Wi-Fi loss, host switching and long streaming sessions.

## Reference implementation

This integration follows:

- XITRIX/Moonlight-Switch issue #254, "Adding wireguard VPN support"
- jmpangilinan/netbird-switch
- jmpangilinan/Moonlight-Switch

The NetBird library is MIT licensed. Artemis/Moonlight remains under its existing GPL license terms.
