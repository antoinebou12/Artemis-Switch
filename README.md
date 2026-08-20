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
[![Docs](https://img.shields.io/badge/docs-GitHub%20Pages-2196F3)](https://antoinebou12.github.io/Artemis-Switch/)

**Native Moonlight-compatible game streaming for Nintendo Switch / Horizon OS.**

<img width="1080" height="607" alt="image" src="https://github.com/user-attachments/assets/8d5c5659-5331-451d-8690-ddab305036ab" />

<img width="1080" height="607" alt="image" src="https://github.com/user-attachments/assets/2dcdb96f-2d12-4d61-84ba-b3957b8a20dc" />

Author: [antoinebou12](https://github.com/antoinebou12)

Artemis Switch is based on [Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch) and keeps its proven Borealis UI, Moonlight/GameStream transport, FFmpeg/NVDEC decode path, deko3d renderer, controller input, and Sunshine compatibility. The fork focuses on Switch-specific video presentation, performance visibility, and cleaner stream controls.

> [!IMPORTANT]
> Artemis Switch is under active development. Portable tests, GitHub Actions jobs, and the Nintendo Switch release build run before release packages are published. Items marked **device validation** still need confirmation on real Sunshine/Apollo streams.

---

## Documentation

Settings help is on GitHub Pages ([IPC Toolkit](https://ipctk.xyz)-style **blue** Material site):

| Page | Contents |
|---|---|
| [Home](https://antoinebou12.github.io/Artemis-Switch/) | What Artemis Switch is |
| [What's new](https://antoinebou12.github.io/Artemis-Switch/whats-new.html) | Pacing, telemetry, schema 13 |
| [Install](https://antoinebou12.github.io/Artemis-Switch/install.html) | NRO path and title redirection |
| [Settings](https://antoinebou12.github.io/Artemis-Switch/settings.html) | Full option reference |
| [Stream profiles](https://antoinebou12.github.io/Artemis-Switch/profiles.html) | Presets, schema, what is stored |
| [In-stream overlay](https://antoinebou12.github.io/Artemis-Switch/overlay.html) | Quick / Options / Performance (+ UML) |
| [Performance & telemetry](https://antoinebou12.github.io/Artemis-Switch/performance.html) | Bitrate, pipeline stages, benchmark |
| [Credits](https://antoinebou12.github.io/Artemis-Switch/credits.html) | Moonlight-Switch, Apollo, #323 |

Source: [`docs/source/`](docs/source/).

---

## Recommended host and clients

Hosts and clients are separate. Pick a **server** on the PC, then a **client** on each device. Artemis uses Moonlight/GameStream for pairing and streaming; host administration stays in the selected host's own web UI.

### Host (PC server)

| Host | Notes |
|---|---|
| **[Vibepollo](https://github.com/Nonary/Vibepollo)** | **Best results in testing.** Apollo-based host with native virtual displays, HDR handling, and turning physical monitors off when a stream starts. |
| **[Vibeshine](https://github.com/Nonary/vibeshine)** | Sunshine-based host with display automation, native virtual displays, and frame-pacing integrations. Artemis Switch connects through the standard Moonlight/GameStream path. |
| **[Punktfunk](https://punktfunk.unom.io/)** | Native-first host with an opt-in Moonlight/GameStream plane. It creates a virtual display matching the resolution and FPS requested by Artemis. |
| **[Apollo](https://github.com/ClassicOldSong/Apollo)** | Strong virtual-display host (SudoVDA). Same GameStream path; use the web UI for display options. |
| [Sunshine](https://github.com/LizardByte/Sunshine) | Compatibility baseline. Works with every Moonlight client; no Apollo-style virtual display extras. |
| [Foundation-Sunshine](https://github.com/AlkaidLab/foundation-sunshine) | Sunshine fork (HDR, audio, encoder changes). Detected by name; treated as standard GameStream. |
| [Polaris](https://github.com/papi-ux/polaris) | Linux-focused host. Detected by name; treated as standard GameStream. |
| [Solar Flare](https://github.com/vindeckyy/Solar-Flare) | Linux/Android-focused host. Detected by name; treated as standard GameStream. |

> [!TIP]
> **Using Vibeshine?** Pair Artemis Switch with the Vibeshine PC just like any other Moonlight-compatible client. Select the host in Artemis Switch, complete the PIN pairing in Vibeshine's web UI, and manage virtual displays, HDR, and other host-side options from Vibeshine.

> [!TIP]
> **Using Punktfunk?** First enable its opt-in GameStream plane by following the official [Connect with Moonlight guide](https://docs.punktfunk.unom.io/docs/moonlight). Pair from `https://<host>:47992/`, then choose resolution and FPS in Artemis; Punktfunk creates the matching virtual display. Artemis advertises the connected Joy-Con / controller slots in the GameStream launch and forwards standard Moonlight gamepad, hot-plug, rumble, and motion events. On Windows, keep Punktfunk's bundled virtual-gamepad driver installed. A native-only Punktfunk host cannot accept Moonlight clients.

### Host integration matrix

| Feature | Vibeshine | Punktfunk |
|---|---|---|
| Pairing and app list | Moonlight/GameStream | Moonlight/GameStream (must be enabled) |
| Requested resolution / FPS | Standard mode plus precise-FPS launch options | Standard GameStream mode; host matches the requested mode |
| Virtual display | Client-requested through advertised Vibeshine launch fields | Host-managed automatically |
| HDR | Host-advertised; requires a compatible capture/codec path | Host-advertised only when its 10-bit capture and encoder path is available |
| Mouse, keyboard, controllers | Standard GameStream input | Standard GameStream input |
| Web console | `https://<host>:47990/` | `https://<host>:47992/` |

Artemis probes only unauthenticated product/version surfaces and stores no host credentials. It does not control Punktfunk's [management API](https://docs.punktfunk.unom.io/openapi.json), Vibeshine scoped API tokens, or either host's native administration plane.

### Client (device)

| Client | Device | Notes |
|---|---|---|
| **[Artemis Switch](https://github.com/antoinebou12/Artemis-Switch)** (this app) | Nintendo Switch | Moonlight-Switch fork with FSR/RCAS, stream profiles, and Switch presentation. |
| **[Artemide](https://github.com/derflacco/moonlight-android)** | Android | Highly recommended on Android: ultra-low latency (ULL), direct presentation, built-in FSR upscaling. FSR Performance / Balanced / Quality presets inspired this fork. |
| [Artemis](https://github.com/ClassicOldSong/moonlight-android) (Moonlight Noir) | Android | Apollo’s companion client (virtual display / host UX inspiration). |
| [Moonlight](https://github.com/moonlight-stream) | PC, Android, iOS, … | Official clients. |
| [Moonlight V+](https://github.com/qiin2333/moonlight-vplus) | Android | Alternative Android fork (frame-gen and extra host tools). |

**Why this stack:** Vibeshine, Vibepollo, Apollo, or GameStream-enabled Punktfunk on the PC plus Artemide on Android (and Artemis Switch on the docked/handheld Switch) is the closest to a console-like session: the host manages display behavior, while the client stays low-latency and can upscale a smaller stream with FSR. Host capabilities vary; Artemis shows the detected integration in Host settings.

---

## Feature overview

| Area | What you get |
|---|---|
| **Stable video presentation** | Fit, Fill, Stretch, Zoom/Pan, Rotation, and Full Range share one Switch presentation path |
| **Video filtering** | FSR/EASU upscaling (FSR1 / SGSR1 / NIS), RCAS sharpening, dithering, Full/Limited range |
| **Stream profiles** | Named full settings profiles (`profile.json`); per-host assign; import/export; 30–120 FPS |
| **Cleaner controls** | Slim Quick Actions; Options for rotation, filters, pointer, host shortcuts, and more |
| **Performance & diagnostics** | Actual vs configured bitrate, queue wait / jitter, receive→present pipeline ms, startup timing, Benchmark export |
| **Switch optimizations** | Adaptive low-latency pacing (deadline + latest-frame-wins), WLAN priority, frame-queue telemetry, NVDEC + deko3d |

---

## Features

### Stable video presentation

Fit, Fill, Stretch, Zoom/Pan, Rotation, and Full Range use one Switch presentation path instead of switching between separate filtered and direct renderers.

| Mode | Behavior |
|---|---|
| **Fit** | Preserves aspect ratio with stable black letterbox / pillarbox regions |
| **Fill** | Crops source UVs to cover the complete output |
| **Stretch** | Maps the complete source to the full output viewport |
| **Zoom / Pan** | Bounded GPU-side source crop with persistent state |
| **Rotation** | 0° / 90° / 180° / 270° stream orientation (overlay Options); correct Fill crop UVs on deko3d |
| **Full Range** | Requests `COLOR_RANGE_FULL` and keeps the video filtering pipeline available |

The presentation path reuses existing NVDEC/NVTEGRA frame mappings and deko3d rendering infrastructure.

### Video filtering

The existing Switch GPU filtering stack remains available with the presentation modes:

| Filter | Role |
|---|---|
| **FSR / EASU** | Upscaling (FSR1; also SGSR1 / NIS modes when enabled) |
| **RCAS** | Sharpening (FSR RCAS) |
| **Dithering** | Temporal / spatial dithering |
| **Full / Limited** | Color-range conversion |

If filtering resources are unavailable, Artemis Switch keeps the selected Fit/Fill/Stretch geometry and falls back safely instead of changing presentation mode.

### Stream profiles

Named stream profiles store a full settings-style snapshot (video, presentation, stream/audio, controller, keyboard, mouse, pointer, keys mapping layout) in `profile.json`. Assign a profile per host; create/edit/delete from Host (RB / Y / X) or manage import/export from the app list.

| Control | Options |
|---|---|
| **FPS** | 30 · 40 · 60 · 90 · 120 (always exposed; no separate high-FPS unlock) |
| **Resolution** | Height presets and optional custom width / height (custom W/H hidden when off) |
| **Scale / HW** | Native resolution scale; hardware decoding |
| **Bitrate** | Exact kbps for the next stream connection |
| **Codec** | H.264 · H.265 · AV1 (where supported) |
| **Presentation** | Fit / Fill / Stretch, upscaling, RCAS, dithering |
| **Input** | Pointer mode, mouse/keyboard options, deadzones, rumble, mapping layout |

### Cleaner controls

| Surface | Contents |
|---|---|
| **Quick Actions** | Keyboard; move window left/right; mouse; volume; touch screen; host shortcuts / server commands with short helper text |
| **Options** | Rotation, scale mode, filters, pointer, live input, audio, and more |
| **Host tabs** | Applications (default) · Host settings (web-config QR, stream profile) |
| **Settings groups** | Stream · Presentation · Advanced stream/network · Motion · VPN |

Presentation settings keep Fit/Fill/Stretch, Rotation, Full Range, Zoom/Pan persistence, and current FSR/RCAS/dithering status together.

### Performance and diagnostics

The Performance page exposes live streaming and renderer information:

| Metric | Description |
|---|---|
| Network | Receive latency, packet loss, Wi‑Fi signal / graph |
| Decode / render | Decode / render latency, host / received / decoded / rendered FPS |
| Frame queue | Current queue depth |
| GPU / presentation | GPU render time and active Fit / Fill / Stretch mode |
| Switch runtime | Operation mode, CPU / GPU / memory clocks, battery |
| Benchmark | Start / stop / save / reset |
| Debug | On-screen debug stats and logs (after Benchmark) |

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
| Zoom & Pan | ✅ Available | GPU source crop with persistent state |
| Rotation | ✅ Available | Overlay Options; deko3d Fill crop UVs corrected for 90° / 270° |
| Full-range video | 🟡 Device validation | Host request + deko3d full-range YUV conversion |
| Named stream profiles | ✅ Integrated | Full settings snapshot, per-host assign, import/export |
| Joy-Con / Pro Controller motion | ✅ Integrated | Existing Moonlight motion forwarding is policy-gated |
| Multiple controllers | ✅ Build + unit tested | Five-player active mask, hot-plug, independent player input |
| Console-motion fallback | ⛔ Disabled by default | API detectable; console motion vectors not mapped safely yet |
| Live performance view | ✅ Integrated | Overlay Performance tab (Benchmark, then Debug / logs) |
| Benchmark runtime | ✅ Integrated | Live sampling, P50/P95/P99, frame-queue faults, stability score |
| Benchmark JSON / CSV | ✅ Integrated | Includes Switch runtime metadata when services are available |
| Apollo capability detection | 🟡 Partial | Conservative detection with Sunshine-safe fallback |
| Apollo virtual display / commands / clipboard | ✅ Gated | Available when the host advertises capability / permissions; applies to Apollo and Vibepollo alike |
| Vibepollo integration | ✅ Distinct identity | Detected as its own host kind, inherits every Apollo-gated extension |
| Polaris / Solar Flare / Foundation-Sunshine | 🟡 Named only | Detected and labelled; no fork-specific extensions claimed |
| Controller battery reporting | ✅ Integrated | Per-pad `LiSendControllerBatteryEvent`, lower Joy-Con of a pair, 30 s throttle |
| Non-US keyboard layouts | ✅ Integrated | AltGr layer for de/fr/es plus the ISO key left of Z, so `\|`, `@`, `\` and `~` are reachable |
| Vibeshine integration | ✅ Capability-gated | Precise FPS and advertised virtual-display launch fields; no assumed Apollo clipboard access |
| Punktfunk integration | ✅ GameStream plane | Product/version probe, host-managed display model, `:47992` console; native protocol/admin APIs excluded |
| French Artemis UI | ✅ Integrated | Settings, overlay tabs, and Performance UI use Borealis i18n |
| CI / Release CD | ✅ Integrated | Unit, sanitizer, i18n, release-contract, Switch NRO publish |

---

## Nintendo Switch

### Architecture

```text
Vibeshine / Vibepollo / Apollo / Sunshine / Punktfunk host
        │
        │ Moonlight/GameStream compatibility plane
        │ + verified host capabilities when available
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
│   ├── Host Applications / Host settings                       │
│   ├── Stream profiles                                         │
│   ├── Performance (Benchmark → Debug)                         │
│   └── Quick Actions                                           │
│                                                               │
│ Switch runtime                                                 │
│   ├── FFmpeg / NVDEC                                          │
│   ├── deko3d (+ Fit/Fill/Stretch/Zoom/Rotation/FSR/RCAS)      │
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
| [Nonary/Vibeshine](https://github.com/Nonary/vibeshine) | Sunshine-fork host (display automation / virtual display) | Host |
| [Punktfunk](https://punktfunk.unom.io/) | Native-first host with opt-in Moonlight/GameStream compatibility | Host |
| [ClassicOldSong/Apollo](https://github.com/ClassicOldSong/Apollo) | Sunshine-fork host (gated extensions) | Host |
| [Nonary/Vibepollo](https://github.com/Nonary/Vibepollo) | Apollo-fork host (virtual display / HDR) | Host |
| [moonlight-stream](https://github.com/moonlight-stream) | Moonlight ecosystem | Protocol |

---

## Known limitations

- Multiple simultaneous controllers need multiplayer confirmation against each supported host.
- Full-range output still benefits from visual verification across host resolutions.
- Console-motion fallback remains disabled until libnx console sensor vectors are mapped safely.
- Apollo virtual-display, server-command, and clipboard operations are capability-gated (shown when the host advertises them).
- Punktfunk's native QUIC protocol and authenticated host administration APIs are outside Artemis Switch's scope; enable its GameStream plane before connecting.
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
| [Nonary/Vibepollo](https://github.com/Nonary/Vibepollo) | Apollo-fork host; best results in testing | Host |
| [ClassicOldSong/moonlight-android](https://github.com/ClassicOldSong/moonlight-android) | Artemis / Moonlight Noir Android client (UX inspiration) | Client |
| [derflacco/moonlight-android](https://github.com/derflacco/moonlight-android) | Artemide Android client (ULL, FSR presets) | Client |
| [qiin2333/moonlight-vplus](https://github.com/qiin2333/moonlight-vplus) | Moonlight V+ Android client (alternative) | Client |
| [LizardByte/Sunshine](https://github.com/LizardByte/Sunshine) | Standard GameStream host | Host |
| [Nonary/Vibeshine](https://github.com/Nonary/vibeshine) | Sunshine-fork host; compatible with Artemis Switch through Moonlight/GameStream | Host |
| [Punktfunk](https://punktfunk.unom.io/) | Native-first host; compatible through its opt-in GameStream plane | Host |
| [moonlight-stream](https://github.com/moonlight-stream) | Moonlight ecosystem / protocol | Protocol |
| [Rock88/moonlight-nx](https://github.com/rock88/moonlight-nx) | Moonlight-NX streaming foundations | Legacy |

### Special thanks

- **XITRIX** — Moonlight-Switch author and original codebase
- **ClassicOldSong** — Apollo host and Artemis (Moonlight Noir) client
- **Nonary** — Vibeshine and Vibepollo hosts (display automation, virtual display, and HDR features)
- **Punktfunk contributors** — GameStream compatibility, host-managed virtual displays, and public host documentation
- **derflacco** — Artemide Android client (ULL, FSR Performance / Balanced / Quality)
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
| [#323](https://github.com/XITRIX/Moonlight-Switch/issues/323) | [@nyanpasu64](https://github.com/nyanpasu64) | Low-latency frame pacing (algorithm inspiration; opt-in) |

Related discussion on Moonlight Qt: [moonlight-qt#1557](https://github.com/moonlight-stream/moonlight-qt/issues/1557) (upscaling).

This fork follows the licensing requirements of Moonlight-Switch and the upstream components it incorporates (including Apollo/Sunshine/GameStream, Borealis, FFmpeg/NVDEC, deko3d, and libnx). See [`LICENSE`](LICENSE) and relevant source-file notices for details.
