# Artemis Switch

Native Moonlight-compatible game streaming for Nintendo Switch / Horizon OS.

Artemis Switch is a fork of [Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch) focused on Switch video presentation, stream profiles, overlay controls, and Apollo host features.

```{toctree}
:hidden:
:maxdepth: 2

whats-new
install
settings
profiles
overlay
overlay-quick
overlay-options
overlay-performance
performance
credits
```

## Documentation

| Page | What it covers |
|---|---|
| [What's new](whats-new.md) | Recent features: pacing, telemetry, schema 13 |
| [Install](install.md) | Put the NRO on the SD card and launch with full RAM |
| [Settings](settings.md) | Every Settings, Artemis, profile, and host option |
| [Stream profiles](profiles.md) | How profiles work, built-in presets, what is / is not stored |
| [In-stream overlay](overlay.md) | Quick / Options / Performance tabs (with UML diagrams) |
| [Performance & telemetry](performance.md) | Bitrate, pipeline stages, queue jitter, benchmark export |
| [Credits](credits.md) | Moonlight-Switch, Apollo, Artemide, #323 pacing |

## Where to start

1. Install Vibepollo, Apollo, or Sunshine on the host PC.
2. Copy `Artemis-Switch.nro` to `sdmc:/switch/Artemis-Switch/`.
3. Launch Homebrew Menu with **title redirection** (full RAM).
4. Pair the host, pick a [stream profile](profiles.md), start a game.
5. Open the overlay ([Quick](overlay-quick.md) / [Options](overlay-options.md) / [Performance](overlay-performance.md)) for live controls and [telemetry](performance.md).

Bitrate and most stream changes apply on the **next** connection. Presentation options and low latency pacing can be toggled from overlay **Options** during a stream.

## Links

- [GitHub repository](https://github.com/antoinebou12/Artemis-Switch)
- [Latest release](https://github.com/antoinebou12/Artemis-Switch/releases/latest)
- [Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch) (upstream, read-only for this fork)
