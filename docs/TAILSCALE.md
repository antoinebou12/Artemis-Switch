# Tailscale remote access on Nintendo Switch

Artemis Switch has a built-in Tailscale client. It joins your tailnet directly from the Switch, so you can stream from a host anywhere on your tailnet without running a VPN on the console or exposing the host to the internet.

The Switch has no system VPN, so Artemis does not route the whole console. It only carries Moonlight traffic (pairing, app list, launch, video, audio and input) to the host you pick.

## What you need

- A Tailscale account (or a compatible control server, see *Custom control server* below).
- The host PC running Sunshine, Apollo or Vibepollo, and either:
  - Tailscale installed on the host PC, **or**
  - a **subnet router** on the host's network (for example Tailscale on an OpenWrt router advertising `192.168.1.0/24`), with the route approved in the admin console.
- An **auth key** for the Switch, created in the [admin console](https://login.tailscale.com/admin/settings/keys).

## Setup

### 1. Create an auth key

In the admin console, open **Settings → Keys → Generate auth key**.

- A **one-off** key is enough. The Switch only needs it the first time; after that it stays registered with its saved identity.
- Pick **Reusable** if you plan to reset the Switch's Tailscale state often.
- If device approval is on for your tailnet, approve the Switch in **Machines** after it first connects.

### 2. Put the key on the SD card

Save the key in a text file on the SD card. The default location is:

```text
sdmc:/switch/Artemis-Switch/tailscale/auth.key
```

The file may contain just the key, or a `key=` line:

```text
# lines starting with # or ; are ignored
auth_key=tskey-auth-xxxxxxxxxxxx-xxxxxxxxxxxxxxxxxxxxxxxx
```

`auth_key`, `authkey` and `key` are all accepted. If you keep the file elsewhere, choose it in Settings (next step). When no file is chosen, Artemis also looks for `tailscale/auth-key.txt`, `tailscale/tailscale.key`, `tailscale.key`, `tailscale.key.txt` and `tailscale.txt` in the Artemis folder.

The key is only read into memory while registering and is wiped afterwards. Only the file location is saved in Settings. You can delete the file once the Switch shows up in your admin console.

### 3. Turn it on

In the Artemis settings, under **Remote Access**:

1. Set **Provider** to **Tailscale (Experimental)**.
2. **One-off auth key file**: shows the key file that was found (`Key found`), or lets you pick one.
3. Select **Connect now**, or turn on **Connect on startup**.

The status row shows the progress: *Connecting to control*, *Control connected*, then *Ready* with the Switch's tailnet IP. The Switch appears in the admin console as `artemis-switch`.

### 4. Add the host

Add the host by address, as you would on a LAN:

| Host setup | Address to enter |
|---|---|
| Tailscale on the host PC | The host's tailnet IP, e.g. `100.98.191.24` |
| Host behind a subnet router | The host's **LAN** IP, e.g. `192.168.1.50` |

Then pair with the PIN as usual. Sunshine shows no popup when pairing succeeds; the host simply appears in the list.

Any address that is not on your tailnet (or behind a subnet router on it) is dialed normally, so LAN hosts keep working while Tailscale is on.

## Settings

| Setting | What it does |
|---|---|
| Provider | `Tailscale (Experimental)` turns the client on. `Off` disconnects. |
| One-off auth key file | Where the auth key is read from at first registration. |
| Direct connections (experimental) | Try a direct UDP path to the host instead of relaying everything through DERP. **Off by default.** See *Direct connections* below. Applies on the next host connection. |
| Prefer LAN | Use the local network when the host is reachable there, even while Tailscale is on. |
| Connect on startup | Connect to the tailnet when Artemis starts. |
| Connect now / Disconnect | Start or stop the client. Disconnect closes everything within about a second. |

## How it works

```text
Moonlight ──127.0.0.1──> loopback proxy ──> lwIP ──> WireGuard ──┬── DERP relay (TLS, default) ──┬──> host PC
                         (TCP + UDP relays)                       └── direct UDP (opt-in) ────────┘    or subnet router

Control plane (TS2021 Noise + HTTP/2) ──> peers, keys, subnet routes, DERP map
```

1. **Control.** Artemis connects to the control server over TLS, upgrades to Tailscale's TS2021 Noise protocol, and speaks HTTP/2 inside it. It registers the node with your auth key (first run only), then holds a long-poll map stream that delivers the list of peers, their keys, addresses, subnet routes and the DERP relay map.
2. **Identity.** The machine, node and disco keys are generated on the Switch with the console's hardware random source and saved in `tailscale/state.bin`. Later starts reuse them, so no new auth key is needed.
3. **Routing a host.** When Moonlight connects to an address, Artemis checks the netmap:
   - a peer's tailnet IP goes to that peer;
   - a LAN IP inside a subnet a peer advertises goes to that peer (the most specific subnet wins), and the router forwards it to the host;
   - anything else is not touched.
4. **The tunnel.** For a routed host, Artemis brings up a userspace WireGuard tunnel to the peer and a small loopback proxy. Moonlight dials `127.0.0.1` on the usual GameStream ports; the proxy carries those connections through an in-app TCP/IP stack (lwIP) and WireGuard to the host. The UDP video, audio and input relays only start once a stream is launched.
5. **The packet path.** WireGuard packets travel through Tailscale's **DERP** relay servers over TLS, in the peer's home region. This works through almost any network but adds latency. With **Direct connections** on, Artemis also looks for a direct UDP path (below).
6. **Staying connected.** The control stream is kept alive with HTTP/2 flow control. If it drops, Artemis reconnects on its own with a 1–30 s backoff, reusing its registration. The DERP relay and WireGuard tunnel are separate, so a running stream keeps going during a control reconnect. The last home DERP region is remembered so peers can reach the Switch right after a reconnect or restart.

### Direct connections (experimental)

DERP carries all traffic by default. Turning on **Direct connections** lets Artemis find a direct path, the way the official Tailscale apps do:

- After the WireGuard handshake succeeds over DERP, Artemis opens one UDP socket (port `41641` if free).
- It learns its LAN address and asks the DERP node's STUN server for its public address, and advertises both to control and to the peer.
- It sends encrypted **disco** pings to the peer's addresses every 3 seconds, answers the peer's pings, and asks the peer to ping back.
- When a ping is answered, WireGuard switches to that address. The path is only trusted for 6.5 seconds after the last answer, so if it stops working, traffic moves back to DERP within a few seconds.

When a direct path works you get lower latency, smoother video on lossy networks (UDP is no longer wrapped in TCP), and no DERP bandwidth limit. Some networks cannot be traversed at all (strict or symmetric NAT, some mobile carriers, hotel or school Wi-Fi); there, traffic simply stays on DERP.

To see whether a network can go direct, run `tailscale ping <host>` from another device on the same network. If it reports `via <ip>:<port>`, a direct path is possible. If the address after `via` is IPv6 (in `[...]`), that path is not available to the Switch, which only has IPv4.

**Mobile data and phone hotspots** usually stay on DERP. Carriers put phones behind carrier-grade NAT (a private `10.x` or `100.x` address), which IPv4 direct paths cannot get through. Phones still often go direct on mobile data, but over IPv6. On the same Wi-Fi as the host, or on a normal home or office network, the Switch normally goes direct within a second.

## Limits

- Only Moonlight traffic to the selected host goes through the tunnel; the rest of the console is unaffected.
- One active host route at a time.
- IPv4 only. The Switch's network stack has no IPv6, so Tailscale IPv6 addresses and IPv6 direct paths cannot be used.
- Exit nodes are ignored (their `0.0.0.0/0` route is skipped), as are Tailscale DNS (MagicDNS) and Tailscale SSH. Enter hosts by IP.
- Direct connections have no UPnP / NAT-PMP port mapping and no hard-NAT workarounds.

The full list of what is and is not implemented is kept in `compatibility/tailscale/manifest.json`.

## Troubleshooting

Artemis writes a detailed log to `sdmc:/switch/Artemis-Switch/vpn.log`. Tailscale lines are tagged `TS` (client and route) and `Remote` (pairing and host saving).

| Symptom | What to check |
|---|---|
| *Authentication failed* | The auth key is used up, expired, or not reusable. Generate a new one. The log line `control wants an interactive browser login` means the key was not accepted. |
| Status stays at *Control connected* | Device approval is on: approve the Switch in the admin console. |
| Host does not respond | Check the host is online in the admin console. For a subnet router, check the route is approved and the address is inside it. Look for `route activation failed` in the log. |
| Pairing works but the host is not saved | Look for `host save:` in the log; it shows why. |
| Stream stutters | DERP is far away or busy. Try **Direct connections**, and look for `WireGuard now goes directly to …` in the log. |
| Direct connections never go direct | Check the `direct path self-test` lines first: `loopback packet received` and `DNS reply from 8.8.8.8` mean the Switch's UDP works. Then a `public endpoint … (STUN)` with no `pong from …` means the network (often a mobile carrier) cannot be traversed; DERP keeps working. |

Useful log lines when things work:

```text
control connect succeeded. Polling netmap stream...
DERP relay connected via derp…  region N
WireGuard handshake with peer completed
TCP proxy ready: 127.0.0.1:{47989,47984,48010} -> 192.168.1.50 via subnet router 100.x.y.z
direct path: public endpoint 203.0.113.7:41641 (STUN)
WireGuard now goes directly to 203.0.113.9:41641 (rtt 12 ms) instead of DERP
```

### Starting over

To make the Switch register as a new device, disconnect, delete `sdmc:/switch/Artemis-Switch/tailscale/state.bin` (and `state.bin.homederp`), put a fresh auth key in place and connect again. Remove the old device from the admin console.

## Custom control server

The control server is set in the `settings` object of `sdmc:/switch/Artemis-Switch/settings.json`:

| Key | Default |
|---|---|
| `tailscale_control_host` | `controlplane.tailscale.com` |
| `tailscale_control_port` | `443` |
| `tailscale_control_public_key` | Tailscale's control key (`mkey:…`) |
| `tailscale_hostname` | `artemis-switch` |

A self-hosted control server (such as Headscale) needs its own host and `mkey:` public key. This setup has not been tested and is not supported.
