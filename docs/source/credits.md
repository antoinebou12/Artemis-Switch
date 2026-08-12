# Credits

Artemis Switch is a fork. Credit **XITRIX/Moonlight-Switch** first, then Apollo, Artemis Classic, Artemide, and issue/PR inspiration used for Switch-specific UX.

## Upstream and hosts

| Project | Role |
|---|---|
| **[XITRIX/Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch)** | Upstream Switch client (Borealis UI, Moonlight transport, NVDEC/deko3d). **Primary credit.** |
| [moonlight-stream](https://github.com/moonlight-stream) | Moonlight / GameStream protocol family |
| [Nonary/Vibepollo](https://github.com/Nonary/Vibepollo) | Recommended host in testing (virtual display / HDR) |
| [ClassicOldSong/Apollo](https://github.com/ClassicOldSong/Apollo) | Virtual-display host; server commands / clipboard |
| [LizardByte/Sunshine](https://github.com/LizardByte/Sunshine) | Compatibility baseline host |

## Clients and UX inspiration

| Project | Inspiration |
|---|---|
| [derflacco/moonlight-android](https://github.com/derflacco/moonlight-android) (Artemide) | FSR Performance / Balanced / Quality presets; low-latency client UX |
| [ClassicOldSong/moonlight-android](https://github.com/ClassicOldSong/moonlight-android) (Artemis / Noir) | Apollo companion UX |
| [qiin2333/moonlight-vplus](https://github.com/qiin2333/moonlight-vplus) | Android fork ideas (not all ported) |

## Overlay feature credits

| Overlay area | Credit |
|---|---|
| [Quick](overlay-quick.md) | Moonlight-Switch overlay base; Apollo for server commands; this fork for focused Quick layout |
| [Options](overlay-options.md) | Moonlight-Switch Options; [#323](https://github.com/XITRIX/Moonlight-Switch/issues/323) [@nyanpasu64](https://github.com/nyanpasu64) for pacing inspiration; Artemide for FSR preset naming |
| [Performance](overlay-performance.md) | Moonlight-Switch stats base; this fork for pipeline / bitrate / benchmark export |

## Issue and PR inspiration

| Link | Author | Used for |
|---|---|---|
| [Moonlight-Switch #323](https://github.com/XITRIX/Moonlight-Switch/issues/323) | [@nyanpasu64](https://github.com/nyanpasu64) | Low-latency frame pacing (opt-in adaptive deadline + latest-frame-wins) |

Upstream Moonlight-Switch is **read-only** for this fork. All shipping work stays in [antoinebou12/Artemis-Switch](https://github.com/antoinebou12/Artemis-Switch).

## License

See the repository [LICENSE](https://github.com/antoinebou12/Artemis-Switch/blob/main/LICENSE). NRO / app author metadata: **antoinebou12**.
