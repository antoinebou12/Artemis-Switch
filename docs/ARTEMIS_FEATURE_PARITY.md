# Artemis Switch → Moonlight-Switch feature parity notes

This document is for **XITRIX** and other Moonlight-Switch maintainers, and for Artemis Switch contributors.

**Thank you first.** Artemis Switch exists because of [Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch). The Borealis UI, GameStream client, FFmpeg/NVDEC path, deko3d renderer, and input stack remain the foundation. Nothing here is a demand to merge Artemis code as-is — only an optional map of Switch-focused ideas that proved useful in the fork, offered as inspiration.

- Upstream: https://github.com/XITRIX/Moonlight-Switch
- Fork: https://github.com/antoinebou12/Artemis-Switch
- Implementation PR (fork only): https://github.com/antoinebou12/Artemis-Switch/pull/22

## Design principles used in Artemis

1. Prefer small, testable helpers over large UI rewrites.
2. Keep Sunshine as the default host path; gate Apollo-only behavior behind capability checks.
3. Prefer read-only diagnostics (clocks, battery, controller state) over write APIs.
4. Do not invent GameStream APIs that do not exist (for example there is no remote “add app”; use the host web UI on `:47990`).

## Complete feature catalog

### Credits / provenance

- About link to Moonlight-Switch (XITRIX) and thank **XITRIX first** (accent color), then the Artemis maintainer, then existing credits.
- Links for Apollo host and Artemis Android (ClassicOldSong) as host/client reference.

### Presentation / video

| Idea | Why it helped on Switch | Pointers |
| --- | --- | --- |
| Unified Fit / Fill / Stretch / Zoom-Pan / Full-range | One presentation path avoids fighting filtered vs direct renderer modes | `DisplayTransform`, `RendererPresentationPolicy`, deko3d texture path |
| Live display transform without swapping renderers | Safer mid-stream tweaks | Settings + overlay Options |
| Coordinate mapper for letterbox / crop / zoom math | Stable geometry across Fit/Fill/Zoom | `DisplayCoordinateMapper` |
| Keep FSR / RCAS / dithering where already present | Do not replace the post-process path with the Performance HUD | Existing deko3d post-process |

### Performance visibility

| Idea | Why it helped on Switch | Pointers |
| --- | --- | --- |
| Lite performance snapshot | Players see lag sources without a heavy overlay | `PerformanceLite`, Performance tab |
| Metrics: network Mbps, receive / decode / render latency, packet loss | Actionable stream health | Session stats → lite snapshot |
| FPS: host / received / decoded / rendered + frame queue | Separates network vs decode vs present issues | Performance tab rows |
| GPU render timing + presentation mode / color range | Ties UI to what the renderer is doing | Performance tab |
| Read-only Switch clocks (CPU / GPU / EMC), mode, battery | Explains docked vs handheld; **no `clkrst` writes** | `collectSwitchRuntimeMetadata()` |
| Show `-` when clocks unavailable | Matches benchmark export semantics | Performance tab |

### Input / controllers

| Idea | Why it helped on Switch | Pointers |
| --- | --- | --- |
| Multi-slot controller diagnostics + live sampling | Detached Joy-Cons show up as separate pads | `ControllerDiagnostics`, overlay diagnostics |
| Rumble low / high / **both motors** / **all pads** | Borealis maps low→left and high→right; both hits both Joy-Cons | `sendRumble(slot, high, low)` |
| Mouse speed true linear **0.1x–2.0x** | Clearer than a mislabeled “acceleration” curve | `Settings::mouse_speed_scale()` |
| Pointer settings / touch control profile | Consistent pointer behavior | `PointerSettings`, `TouchControlProfile` |
| Switch motion policy gating | Capability-aware motion forwarding | `SwitchMotionPolicy` |
| Host keyboard shortcuts + capability-gated server commands | Apollo/Sunshine extras without breaking plain Sunshine | `HostKeyboardShortcuts`, `HostCapabilities` |

### Stream UX

| Idea | Why it helped on Switch | Pointers |
| --- | --- | --- |
| Slim Quick Actions | Less clutter mid-stream | Overlay Quick tab |
| Quick: keyboard, move window L/R, host shortcuts, server commands, mouse | Separates one-shot actions from deep options | `quick_tab.xml`, overlay |
| Options keeps deeper tuning | Virtual-display style choices stay out of live Quick | Options tab / Artemis settings |
| Always-on FPS 30 / 40 / 60 / 90 / 120 | No unlock gate for high FPS entries | Settings FPS list |
| Handheld 1280×720 and Docked 1920×1080 presets | Fast docked/handheld switch in settings | Artemis settings presets |
| Host web config `https://<host>:47990/` | Honest “add apps on the PC” path (no fake `gs_add_app`) | App list + Host tab |

### Apollo (optional / gated)

| Idea | Why it helped | Pointers |
| --- | --- | --- |
| Capability detection before Apollo-only UI | Avoids dead controls on Sunshine | `HostCapabilities`, Apollo host options store |

### Fork packaging (Artemis-Switch only; not requested upstream)

- NRO branding Artemis Switch, version **1.5.3**, About/version strings.
- Portable unit tests + i18n consistency (en-US + fr) for new strings.
- Multi-OS CI on the fork: portable Linux tests/sanitizers + All Builds (Linux, Windows, macOS, Android, Vita, Switch).

## What is *not* requested for Moonlight-Switch mainline

- Auto-tune / session-fighting performance writers
- Fake GameStream catalog write / remote “add app” API
- Clock overclocking (`clkrst` writes)
- Requirement to adopt Apollo UI in mainline
- Merging the Artemis runtime/codebase as-is into XITRIX

## Suggested gentle adoption order (if any)

1. Credits / About provenance
2. Performance lite rows (UI + existing session stats)
3. Mouse speed remap + label clarity
4. Controller diagnostics / dual-motor rumble
5. Presentation policy helpers (larger review surface)
6. Apollo capability gating (only if desired)

## Contact

- Artemis Switch fork maintainer: **antoinebou12**
- Original Moonlight-Switch author: **XITRIX** — thank you again
