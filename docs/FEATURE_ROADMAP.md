# Feature roadmap (scoped)

This note tracks **larger** Moonlight-Switch community requests that are **not**
shipped as full runtime features yet on Artemis Switch. Smaller PRs may land
incrementally; this document is the scope map.

Fork: https://github.com/antoinebou12/Artemis-Switch  
Upstream: https://github.com/XITRIX/Moonlight-Switch

## AV1 codec (upstream #252)

**Ask:** Select AV1 in decoder settings for lower bitrate at a given quality
(community tables often cite ~2662 kbps for Switch OLED 720p60 AV1).

**Current state**

- `VideoCodec::AV1` exists in settings types / i18n (`settings/av1`).
- The Settings codec list still exposes H.264 / HEVC only on Switch.
- Switch has **no hardware AV1 decode**. Software AV1 may work at low
  resolutions but is expected to be CPU-heavy versus NVDEC H.264/HEVC.

**Planned scope (separate PR later)**

1. Optionally show AV1 in the codec list with an “Experimental / software”
   subtitle on Switch.
2. Guard session start if the decoder cannot open an AV1 codec (clear error).
3. Document recommended resolution/bitrate caps for software AV1.

**Not planned:** Claiming hardware AV1 on Switch.

## Spatial upscalers (SGSR1 / NIS / FSR1)

**Ask:** Upscale lower stream resolutions on a higher-res display, inspired by
[moonlight-qt#1557](https://github.com/moonlight-stream/moonlight-qt/pull/1557)
(SGSR1 / NIS / FSR1 paths).

**Current state**

- Existing FSR1 / RCAS / dithering path behind `ENABLE_UPSCALING` on some
  renderers.
- Switch deko3d presentation path already focuses on Fit/Fill/Stretch /
  zoom-pan; post-process upscalers are not the default Switch NRO path.

**Suggested platform mapping (from the moonlight-qt discussion)**

| Renderer | Prefer | Fallback |
| --- | --- | --- |
| deko3d (Switch / NVIDIA) | NIS (1-pass) | SGSR1 if latency budget exceeded |
| OpenGL | NIS or SGSR1 | FSR1 EASU+RCAS (2-pass) |
| Metal | MetalFX RGB | FSR1 shaders |

**Planned scope (separate PRs later)**

1. Docs + settings stub for upscaler mode (`Off` / `SGSR1` / `NIS` / `FSR1`).
2. deko3d NIS or SGSR1 shader integration with measurable frame-time cost.
3. Keep FSR1 where already present; do not regress Fit/Fill presentation.

## Apollo: restart server / reset display

**Ask:** Host actions such as restart Sunshine/Apollo or reset the virtual
display without leaving the Switch client.

**Current state**

- Apollo **server commands** are advertised by the host and surfaced in the
  in-game Overlay → Quick → Server commands when
  `HostCapabilities::serverCommands` is true.
- Virtual display apply/rollback already restarts the Moonlight session with
  Apollo launch options when the host supports it.
- There is **no** hardcoded “Restart server” or “Reset display” button that
  invents GameStream APIs; actions come from the host command list.

**Guidance**

- Prefer host-advertised `ServerCommand` entries for restart/reset.
- Do not add fake GameStream endpoints for “restart host OS” or “reset GPU”.
- If a common Apollo command name becomes stable, we can add a Quick Action
  alias that maps to that advertised command only when present.

## Related smaller PRs (already split)

| Topic | Fork PR |
| --- | --- |
| UI language selection | [#25](https://github.com/antoinebou12/Artemis-Switch/pull/25) |
| Keyboard layout + Korean 2-set | [#26](https://github.com/antoinebou12/Artemis-Switch/pull/26) |
| Hostname / domain + port | [#27](https://github.com/antoinebou12/Artemis-Switch/pull/27) |
| Debug stats corner | [#28](https://github.com/antoinebou12/Artemis-Switch/pull/28) |

Localization file drops (DE/NO/… copy) remain separate from the language
picker; the picker only lists locales that exist under `resources/i18n/`.
