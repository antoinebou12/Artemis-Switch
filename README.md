# Artemis Switch

[![Unit Tests](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/unit-tests.yml/badge.svg?branch=main)](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/unit-tests.yml)
[![Feature & Integration CI](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/feature-integration-ci.yml/badge.svg?branch=main)](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/feature-integration-ci.yml)
[![All Builds](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/all-builds.yml/badge.svg?branch=main)](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/all-builds.yml)
[![Release CD](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/release.yml/badge.svg)](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/release.yml)
[![Latest Release](https://img.shields.io/github/v/release/antoinebou12/Artemis-Switch?display_name=tag&sort=semver)](https://github.com/antoinebou12/Artemis-Switch/releases/latest)
[![License](https://img.shields.io/github/license/antoinebou12/Artemis-Switch)](LICENSE)
[![Stars](https://img.shields.io/github/stars/antoinebou12/Artemis-Switch?style=flat)](https://github.com/antoinebou12/Artemis-Switch/stargazers)
[![Platform](https://img.shields.io/badge/platform-Nintendo%20Switch-E60012)](#nintendo-switch)
[![Moonlight](https://img.shields.io/badge/compatible-Moonlight%20%2F%20Sunshine-4caf50)](#compatibility)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C)](CMakeLists.txt)
[![Languages](https://img.shields.io/badge/UI-English%20%7C%20Fran%C3%A7ais-4c8bf5)](#localization)
[![Status](https://img.shields.io/badge/status-experimental-orange)](#project-status)

**Native Moonlight-compatible game streaming for Nintendo Switch / Horizon OS.**

<img width="1080" height="607" alt="image" src="https://github.com/user-attachments/assets/8d5c5659-5331-451d-8690-ddab305036ab" />

<img width="1080" height="607" alt="image" src="https://github.com/user-attachments/assets/2dcdb96f-2d12-4d61-84ba-b3957b8a20dc" />

Author: [antoinebou12](https://github.com/antoinebou12)

Artemis Switch is based on [Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch) and keeps its proven Borealis UI, Moonlight/GameStream transport, FFmpeg/NVDEC decode path, deko3d renderer, controller input, and Sunshine compatibility. The fork focuses on Switch-specific video presentation, performance visibility, and cleaner stream controls.

> [!IMPORTANT]
> Artemis Switch is under active development. Portable tests, GitHub Actions jobs, and the Nintendo Switch release build run before release packages are published. Items marked **device validation** still need confirmation on real Sunshine/Apollo streams.

---

## Feature overview

| Area | What you get |
|---|---|
| **Stable video presentation** | Fit, Fill, Stretch, Zoom/Pan, and Full Range share one Switch presentation path |
| **Video filtering** | FSR/EASU upscaling, RCAS sharpening, dithering, Full/Limited range conversion |
| **Stream profiles** | 30 / 40 / 60 / 90 / 120 FPS always available; custom resolution and exact bitrate |
| **Cleaner controls** | Quick Actions for frequent in-session tools; Options for live input/audio/filters/diagnostics |
| **Performance & diagnostics** | Live latency, packet loss, render timing, frame queue, presentation mode, Auto Tune |
| **Switch optimizations** | WLAN priority mode, frame-queue telemetry, motion/controller policy, NVDEC + deko3d |

---

## Features

### Stable video presentation

Fit, Fill, Stretch, Zoom/Pan, and Full Range use one Switch presentation path instead of switching between separate filtered and direct renderers.

| Mode | Behavior |
|---|---|
| **Fit** | Preserves aspect ratio with stable black letterbox / pillarbox regions |
| **Fill** | Crops source UVs to cover the complete output |
| **Stretch** | Maps the complete source to the full output viewport |
| **Zoom / Pan** | Bounded GPU-side source crop with persistent state |
| **Full Range** | Requests `COLOR_RANGE_FULL` and keeps the video filtering pipeline available |

The presentation path reuses existing NVDEC/NVTEGRA frame mappings and deko3d rendering infrastructure.

### Video filtering

The existing Switch GPU filtering stack remains available with the presentation modes:

| Filter | Role |
|---|---|
| **FSR / EASU** | Upscaling (enabled only when required GPU resources are valid) |
| **RCAS** | Sharpening |
| **Dithering** | Temporal / spatial dithering |
| **Full / Limited** | Color-range conversion |

If filtering resources are unavailable, Artemis Switch keeps the selected Fit/Fill/Stretch geometry and falls back safely instead of changing presentation mode.

### Stream profiles

| Control | Options |
|---|---|
| **FPS** | 30 · 40 · 60 · 90 · 120 (always exposed; no separate high-FPS unlock) |
| **Resolution** | Custom width / height override from settings |
| **Bitrate** | Exact 1–100 Mbps configuration for the next stream connection |
| **Codec** | H.264 and HEVC via the existing decoder path |

### Cleaner controls

| Surface | Contents |
|---|---|
| **Quick Actions** | Keyboard, performance overlay, pointer/mouse mode, disconnect, quit host app when available |
| **Options** | Live input, audio, image filtering, benchmark, and diagnostic controls |
| **Settings groups** | Stream profile · Presentation · Advanced stream/network · Motion |

Presentation settings keep Fit/Fill/Stretch, Full Range, Zoom/Pan persistence, and current FSR/RCAS/dithering status together.

### Performance and diagnostics

The Performance page exposes live streaming and renderer information:

| Metric | Description |
|---|---|
| Stream config | Resolution, FPS, codec, bitrate |
| Network | Receive latency, packet loss |
| Decode / render | Decode latency, rendered FPS, frame render time |
| Post-process | Post-processing, FSR, RCAS, dithering, GPU render time |
| Frame queue | Current / target / capacity |
| Presentation | Active Fit / Fill / Stretch mode and Full / Limited range |
| Tools | Benchmark controls, Auto Tune |

### Nintendo Switch optimizations

| Optimization | Detail |
|---|---|
| WLAN priority | Optimized WLAN priority mode for streaming |
| Frame queue | Switch-oriented frame-queue telemetry |
| Motion / input | Switch motion and controller policy controls |
| Decode | Existing NVDEC hardware decoding |
| Render | deko3d GPU rendering and post-processing |

---

## Project status

| Area | Status | Current state |
|---|---|---|
| Standard Moonlight / Sunshine streaming | ✅ Available | GameStream remains the compatibility path |
| Switch NVDEC + deko3d | ✅ Build tested | Existing renderer preserved; Artemis presentation path integrated |
| Fit / Fill / Stretch | ✅ Build + unit tested | Fill and Stretch retain filtering; Fit uses a black letterbox viewport |
| Zoom & Pan | 🟡 Device validation | GPU source crop with persistent state |
| Full-range video | 🟡 Device validation | Host request + deko3d full-range YUV conversion |
| Joy-Con / Pro Controller motion | ✅ Integrated | Existing Moonlight motion forwarding is policy-gated |
| Multiple controllers | ✅ Build + unit tested | Five-player active mask, hot-plug, independent player input |
| Console-motion fallback | ⛔ Disabled by default | API detectable; console motion vectors not mapped safely yet |
| Live performance view | ✅ Integrated | Existing in-game Borealis overlay |
| Benchmark runtime | ✅ Integrated | Live sampling, P50/P95/P99, frame-queue faults, stability score |
| Benchmark JSON / CSV | ✅ Integrated | Includes Switch runtime metadata when services are available |
| Apollo capability detection | 🟡 Partial | Conservative detection with Sunshine-safe fallback |
| Apollo virtual display / commands / clipboard | ⏳ Planned | Only verified Apollo protocol operations will be added |
| French Artemis UI | ✅ Integrated | Settings, overlay tabs, and Performance UI use Borealis i18n |
| CI / Release CD | ✅ Integrated | Unit, sanitizer, i18n, release-contract, Switch NRO publish |

---

## Nintendo Switch

### Architecture

```text
Sunshine / Apollo host
        │
        │ Moonlight GameStream
        │ + verified Apollo extensions when available
        ▼
┌──────────────────────── Artemis Switch ────────────────────────┐
│ MoonlightSession                                               │
│   ├── stream configuration                                     │
│   ├── custom resolution / bitrate / FPS                        │
│   └── live session telemetry                                   │
│                                                               │
│ Benchmark                                                      │
│   ├── receive / decode / render / queue metrics                │
│   ├── P50 / P95 / P99 + stability                             │
│   └── JSON / CSV + Switch metadata                             │
│                                                               │
│ Borealis UI                                                    │
│   ├── Artemis settings                                        │
│   ├── Performance                                             │
│   └── Quick Actions                                           │
│                                                               │
│ Switch runtime                                                 │
│   ├── FFmpeg / NVDEC                                          │
│   ├── deko3d (+ FSR / RCAS / dither when available)           │
│   └── Joy-Con / controller input                              │
└───────────────────────────────────────────────────────────────┘
```

### Motion sensors

Joy-Con and compatible controller motion continues to use Moonlight-Switch's existing controller-motion path.

**Console-motion fallback is OFF by default.** Artemis can detect whether the libnx SevenSixAxis API is available, but does not treat opaque/undocumented sensor fields as acceleration or gyro data until vectors are mapped and validated.

---

## Installing

### Switch

1. Download the latest [Artemis Switch release](https://github.com/antoinebou12/Artemis-Switch/releases/latest).
2. Put `Artemis-Switch.nro` on the SD card at:

```text
sdmc:/switch/Artemis-Switch/Artemis-Switch.nro
```

3. Launch **hbmenu** over Title Redirection (for full RAM access).
4. Launch Artemis Switch.

For the easiest local install, extract `dist/nro/Artemis-Switch-SD.zip` to the root of the Switch SD card. The same instructions are included in `dist/nro/README-INSTALL.txt`.

> [!TIP]
> High bitrate at 1080p benefits from CPU/GPU overclocking (for example via [sys-clk](https://github.com/retronx-team/sys-clk) or Atmosphere builds that include clock control). Overclocking is outside Artemis Switch's scope and is at your own risk.

> [!WARNING]
> The author is not responsible for damage to your console if overclocking or unofficial firmware use goes wrong. Think carefully and take responsibility for what you do with your devices.

---

## Controls

| Input | Behavior |
|---|---|
| **Touch** | Move cursor; tap for left click; two-finger scroll |
| **While touching** | ZR / ZL act as left / right mouse; L / R sticks act as scroll wheel |
| **USB mouse / keyboard** | Supported |
| **On-screen keyboard** | Three-finger tap (or Quick Actions) |
| **Gamepad** | Switch pad mapped as X360-style (A/B and X/Y swapped by default); remapping in settings |
| **Multiplayer** | Up to 5 gamepads (including handheld); half Joy-Con supported |
| **SixAxis** | Configure Sunshine to expose a DS4-style controller for gyro / accelerometer (player 1) |
| **In-game overlay** | `-` + `+` together by default (or hold ESC on keyboard); combination and hold time are configurable |

---

## Localization

| Language | Artemis-specific UI |
|---|---|
| English | ✅ |
| Français | ✅ |
| Other upstream Moonlight-Switch languages | ↩️ Existing / fallback strings until Artemis keys are translated |

Language follows system settings (and in-app selection where available). French covers Artemis settings, presentation/motion options, overlay tabs, Performance controls, and benchmark actions.

---

## Continuous Integration

| Gate | Purpose |
|---|---|
| **Unit Tests** | Portable C++20 policy, controller topology, statistics, export, stream-profile, geometry |
| **ASan + UBSan** | Memory and undefined-behavior checks for the portable feature suite |
| **Localization contract** | Every Artemis key in C++/XML must exist in English and French |
| **Release contract** | CD workflow still requires NRO, ELF, source archives, and checksums |
| **Release package dry run** | Builds and inspects `.tar.gz` / `.zip` source bundles |
| **Feature & Integration CI** | Unit suite plus cross-feature integration tests |
| **All Builds** | Platform matrix including real Nintendo Switch `.nro` / `.elf` |
| **Release CD** | Quality gates + Switch build before publishing a `v*` GitHub Release |

Moonlight/GameStream negotiates bitrate during setup. Artemis saves bitrate changes for the **next** stream connection and does not restart an active stream.

---

## Automatic releases

`.github/workflows/release.yml` implements GitHub release CD.

### Dry run

**Actions → Release Artemis Switch → Run workflow** on a branch runs the quality gate, Switch NRO/ELF build, and source packaging validation without publishing a GitHub Release.

### Publish a release

```bash
git tag v0.1.0
git push origin v0.1.0
```

Every tagged release receives:

| Asset | Purpose |
|---|---|
| `Artemis-Switch.nro` | Installable Nintendo Switch homebrew binary |
| `Artemis-Switch.elf` | Debug / symbol build artifact |
| `Artemis-Switch-X.Y.Z-source.tar.gz` | Source bundle including submodule contents |
| `Artemis-Switch-X.Y.Z-source.zip` | ZIP variant of the source bundle |
| `SHA256SUMS.txt` | SHA-256 checksums for all release artifacts |

Tags such as `v0.1.0-beta.1` are published as **pre-releases**. Re-running a tag workflow refreshes assets with `--clobber`.

`SOURCE_INFO.txt` records the project name, author `antoinebou12`, version, commit, and generation time.

---

## Build

Clone recursively with a standard [devkitPro](https://devkitpro.org/wiki/Getting_Started) Switch environment:

```bash
git clone --recursive https://github.com/antoinebou12/Artemis-Switch.git
cd Artemis-Switch
cmake -B build/switch -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON
cmake --build build/switch --target Moonlight.nro --parallel
```

`Moonlight.nro` and `Moonlight.elf` remain internal upstream-compatible build targets. CI exports user-facing artifacts as:

| Artifact | Role |
|---|---|
| `Artemis-Switch.nro` | Release / install binary |
| `Artemis-Switch.elf` | Symbols / debug |

### Validation

```bash
python tests/i18n_consistency_test.py
python tests/release_contract_test.py
cmake -S tests -B build/tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests --parallel
ctest --test-dir build/tests --output-on-failure
```

Cross-feature integration:

```bash
cmake -S ci/integration -B build/integration -DCMAKE_BUILD_TYPE=Debug
cmake --build build/integration --parallel
ctest --test-dir build/integration --output-on-failure
```

### Other platforms (Linux, Windows, macOS, Android, Vita)

Artemis is Switch-first. Desktop and other ports keep Sunshine/GameStream streaming; Switch-only behaviors are **off by default** via CMake feature flags.

See **[docs/OTHER_PLATFORMS.md](docs/OTHER_PLATFORMS.md)** for flags, build sketches, and Linux packaging docs.

---

## Safety and clocks

Artemis benchmark metadata performs **read-only** clock queries. It does not set CPU, GPU, or EMC/RAM clocks. If the relevant service is unavailable, clock metadata remains unavailable/zero rather than modifying the system.

---

## Compatibility

Sunshine and standard Moonlight/GameStream behavior remain the compatibility baseline. Internal upstream build targets and the legacy settings directory are retained where required so existing installations do not lose saved configuration.

| Project | Role | Link |
|---|---|---|
| [XITRIX/Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch) | Upstream Switch client | Base |
| [LizardByte/Sunshine](https://github.com/LizardByte/Sunshine) | Standard GameStream host | Host |
| [ClassicOldSong/Apollo](https://github.com/ClassicOldSong/Apollo) | Sunshine-fork host (gated extensions) | Host |
| [moonlight-stream](https://github.com/moonlight-stream) | Moonlight ecosystem | Protocol |

---

## Known limitations

- Multiple simultaneous controllers need multiplayer confirmation against each supported host.
- Fit, Zoom/Pan, and full-range output still benefit from visual verification across host resolutions.
- Console-motion fallback remains disabled until libnx console sensor vectors are mapped safely.
- Apollo virtual-display, server-command, and clipboard operations stay gated until verified.
- Benchmark scoring still needs tuning from real Switch measurements.

See [`docs/ARTEMIS_SWITCH_INTEGRATION_PLAN.md`](docs/ARTEMIS_SWITCH_INTEGRATION_PLAN.md) for release acceptance criteria.

---

## Credits and license

Artemis Switch is a fork of [Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch) by **XITRIX**. The original Moonlight-Switch source, Borealis UI, Moonlight/GameStream client, FFmpeg/NVDEC decode path, deko3d renderer, and input stack remain the foundation. Existing upstream license and source-file notice requirements remain applicable — see [`LICENSE`](LICENSE) and notices in individual source files.

### Original projects

| Project | Role | Link |
|---|---|---|
| [XITRIX/Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch) | Original Switch client (this fork’s base) | Upstream |
| [ClassicOldSong/Apollo](https://github.com/ClassicOldSong/Apollo) | Sunshine-fork host (capability-gated extensions) | Host |
| [ClassicOldSong/moonlight-android](https://github.com/ClassicOldSong/moonlight-android) | Artemis / Moonlight Noir Android client (UX inspiration) | Client |
| [LizardByte/Sunshine](https://github.com/LizardByte/Sunshine) | Standard GameStream host | Host |
| [moonlight-stream](https://github.com/moonlight-stream) | Moonlight ecosystem / protocol | Protocol |
| [Rock88/moonlight-nx](https://github.com/rock88/moonlight-nx) | Moonlight-NX streaming foundations | Legacy |

### Special thanks

- **XITRIX** — Moonlight-Switch author and original codebase
- **ClassicOldSong** — Apollo host and Artemis (Moonlight Noir) client
- **Rock88** / Moonlight-NX — streaming foundations reused in Moonlight-Switch
- **Natinusala** / **Xfangfang** — Borealis (including later ports)
- **Moonlight team** — GameStream client and protocol work
- **Cameron Gutman** — LibNX guidance
- **Averne** — NVDEC in FFmpeg and guidance
- **Cooler3D** — deko3d help and performance work
- **AMD GPUOpen** — FidelityFX FSR 1.0 EASU / RCAS reference (MIT)
- **antoinebou12** — Artemis Switch fork maintainer

### Moonlight-Switch community feature ideas

Several Artemis Switch features were implemented from ideas raised on [XITRIX/Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch) issues (and related discussion). Credit to the reporters — implementations live in this fork only unless noted otherwise.

| Upstream issue | Reporter | Idea used in Artemis Switch |
|---|---|---|
| [#110](https://github.com/XITRIX/Moonlight-Switch/issues/110) | [@benwa](https://github.com/benwa) | Swap X/Y for starring in the app list |
| [#195](https://github.com/XITRIX/Moonlight-Switch/issues/195) | [@orcavia](https://github.com/orcavia) | Disable swipe-to-open overlay |
| [#163](https://github.com/XITRIX/Moonlight-Switch/issues/163) | [@antegorov](https://github.com/antegorov) | Swap left/right sticks in mouse mode |
| [#70](https://github.com/XITRIX/Moonlight-Switch/issues/70) | [@xyourmomx](https://github.com/xyourmomx) | Home/away style host addressing (multi-endpoint) |
| [#112](https://github.com/XITRIX/Moonlight-Switch/issues/112) | [@xenophobentx](https://github.com/xenophobentx) | 5.1 surround audio option |
| [#295](https://github.com/XITRIX/Moonlight-Switch/issues/295) | [@linckosz](https://github.com/linckosz) | FSR1-like / additional upscaling modes |
| [#254](https://github.com/XITRIX/Moonlight-Switch/issues/254) | [@JoJuStudio](https://github.com/JoJuStudio) | In-app WireGuard VPN support |
| [#250](https://github.com/XITRIX/Moonlight-Switch/issues/250) | [@bengrahamreview](https://github.com/bengrahamreview) | Connecting outside the home network |
| [#144](https://github.com/XITRIX/Moonlight-Switch/issues/144) | [@MetalGooseSolid](https://github.com/MetalGooseSolid) | Bake Tailscale/VPN-style remote access into the client |
| [#24](https://github.com/XITRIX/Moonlight-Switch/issues/24) / [#165](https://github.com/XITRIX/Moonlight-Switch/issues/165) | [@windraver](https://github.com/windraver) / [@Msv777](https://github.com/Msv777) | Host app terminate / disconnect behavior |
| [#252](https://github.com/XITRIX/Moonlight-Switch/issues/252) | [@Befeeter](https://github.com/Befeeter) | Experimental AV1 codec support |
| [#58](https://github.com/XITRIX/Moonlight-Switch/issues/58) | [@gcseed](https://github.com/gcseed) | Debug / FPS overlay corner placement |
| [#239](https://github.com/XITRIX/Moonlight-Switch/issues/239) / [#283](https://github.com/XITRIX/Moonlight-Switch/issues/283) | [@HackZy01](https://github.com/HackZy01) / [@AquaSteam](https://github.com/AquaSteam) | UI language selection and keyboard layout |
| [#266](https://github.com/XITRIX/Moonlight-Switch/issues/266) | [@Moby812](https://github.com/Moby812) | Virtual display (Apollo-gated path) |

Related discussion on Moonlight Qt: [moonlight-qt#1557](https://github.com/moonlight-stream/moonlight-qt/issues/1557) (upscaling).

This fork follows the licensing requirements of Moonlight-Switch and the upstream components it incorporates (including Apollo/Sunshine/GameStream, Borealis, FFmpeg/NVDEC, deko3d, and libnx). See [`LICENSE`](LICENSE) and relevant source-file notices for details.
