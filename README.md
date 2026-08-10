# Artemis Switch

[![Unit Tests](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/unit-tests.yml/badge.svg?branch=main)](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/unit-tests.yml)
[![Feature & Integration CI](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/feature-integration-ci.yml/badge.svg?branch=main)](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/feature-integration-ci.yml)
[![All Builds](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/all-builds.yml/badge.svg?branch=main)](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/all-builds.yml)
[![Release CD](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/release.yml/badge.svg)](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/release.yml)
[![Latest Release](https://img.shields.io/github/v/release/antoinebou12/Artemis-Switch?display_name=tag&sort=semver)](https://github.com/antoinebou12/Artemis-Switch/releases)
[![Platform](https://img.shields.io/badge/platform-Nintendo%20Switch-E60012)](#nintendo-switch)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C)](CMakeLists.txt)
[![Languages](https://img.shields.io/badge/UI-English%20%7C%20Fran%C3%A7ais-4c8bf5)](#localization)
[![Status](https://img.shields.io/badge/status-experimental-orange)](#project-status)

**Artemis Switch** is a native Horizon OS game-streaming client for Nintendo Switch, built on [Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch) and focused on Switch-specific streaming diagnostics, tuning, presentation controls, and carefully gated Apollo integration.

It keeps the existing Moonlight-Switch architecture instead of replacing it: **Borealis UI, Moonlight/GameStream, FFmpeg/NVDEC, deko3d, Joy-Con input, Sunshine compatibility, and the existing post-processing path remain the foundation.**

> [!IMPORTANT]
> Artemis Switch is under active development. Portable tests, local GitHub Actions jobs, and the Nintendo Switch release build are run before the `dist/nro` package is refreshed. Items explicitly marked **hardware validation** still need confirmation during a real Apollo/Sunshine stream.

## Project status

| Area | Status | Current state |
|---|---|---|
| Standard Moonlight / Sunshine streaming | ✅ Available | Standard GameStream remains the compatibility path |
| Switch NVDEC + deko3d | ✅ Build tested | Existing renderer preserved; Artemis presentation path integrated |
| Fit / Fill / Stretch | ✅ Build + unit tested | Fill and Stretch retain filtering; Fit uses a black letterbox viewport |
| Zoom & Pan | 🟡 Device validation | GPU source crop with persistent state |
| Full-range video | 🟡 Device validation | Requested from host and applied with deko3d full-range YUV conversion |
| Joy-Con / Pro Controller motion | ✅ Integrated | Existing Moonlight motion forwarding is policy-gated |
| Multiple controllers | ✅ Build + unit tested | Five-player active mask, hot-plug arrivals/releases and independent player input |
| Console-motion fallback | ⛔ Disabled by default | libnx SevenSixAxis API can be detected, but console motion vectors are not mapped safely yet |
| Live performance view | ✅ Integrated | Uses the existing in-game Borealis overlay |
| Benchmark runtime | ✅ Integrated | Live sampling, P50/P95/P99, frame-queue faults, stability score |
| Benchmark JSON / CSV | ✅ Integrated | Includes Switch runtime metadata when services are available |
| Apollo capability detection | 🟡 Partial | Conservative detection with Sunshine-safe fallback |
| Apollo virtual display / commands / clipboard | ⏳ Planned | Only verified Apollo protocol operations will be added |
| French Artemis UI | ✅ Integrated | Artemis settings, overlay tabs and Performance UI use Borealis i18n |
| CI | ✅ Integrated | Unit, sanitizer, localization, release-contract and cross-feature gates are defined |
| Release CD | ✅ Integrated | `v*` tags build/test/publish NRO, ELF, source bundles and SHA-256 checksums |

## Features

| Feature | Artemis Switch implementation |
|---|---|
| Custom resolution | Persistent width / height override used by `MoonlightSession` |
| Exact bitrate | 1–100 Mbps configuration saved for the next stream connection |
| Frame rates | 30 / 40 / 60 FPS plus gated 90 / 120 FPS options |
| Video codecs | H.264 and HEVC, using the existing decoder path |
| Presentation | Fit, Fill, Stretch, Zoom and Pan on the Switch deko3d path |
| Full range | Host color-range negotiation plus Switch-side deko3d conversion |
| Motion | Joy-Con / controller gyro and accelerometer forwarding policy |
| Performance | Compact bitrate, Switch Wi-Fi history, latency, packet-loss and FPS dashboard |
| Benchmark | Mean, median, P95, P99 and stability scoring |
| Export | JSON + CSV files saved under the Artemis working directory |
| Switch metadata | Handheld/docked mode, battery, charging state and best-effort read-only CPU/GPU/EMC clocks |
| Quick tab | A separate lightweight tab for keyboard, move active PC window, display, controllers, mouse, touch, disconnect and quit-host actions |
| Move game window | Sends `Win + Shift + Left/Right` so the focused Windows game can be pulled onto the Apollo/Sunshine display |
| Controller list | Shows each local player, mapped PC slot, rumble and motion capability in the Quick tab |
| Host capabilities | Sunshine-safe defaults with conservative Apollo feature gating |

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
│   ├── deko3d                                                  │
│   └── Joy-Con / controller input                              │
└───────────────────────────────────────────────────────────────┘
```

### deko3d presentation

The full-screen **Fill** and **Stretch** paths keep the existing Moonlight-Switch renderer behavior, including NVDEC mappings and the FSR/RCAS/dithering pipeline.

When Artemis presentation features are requested, the Switch renderer can use the tested presentation geometry directly:

- **Fit:** preserve aspect ratio and clear unused framebuffer regions to black.
- **Fill:** crop source UVs to fill the destination, then use the enabled filtering stages.
- **Stretch:** preserve the full source, use the full destination, and use the enabled filtering stages.
- **Zoom/Pan:** apply bounded GPU-side source cropping.
- **Full range:** request full-range video from the host and use the corresponding deko3d YUV matrix.

Fit and non-default Zoom/Pan require the direct destination-viewport path. Fill and Stretch use the complete filtered full-screen path.

### Motion sensors

Joy-Con and compatible controller motion continues to use Moonlight-Switch's existing controller-motion path.

**Console-motion fallback is OFF by default.** Artemis can detect whether the libnx SevenSixAxis API is available, but does not treat opaque/undocumented sensor fields as acceleration or gyro data. The UI therefore remains disabled until the console-motion vectors are mapped and validated unambiguously.

## Benchmark

A benchmark samples the active stream instead of using synthetic data. Results include frame rate, receive/decode/render timing, packet loss, frame-queue behavior and percentile statistics.

## Localization

Artemis-specific UI uses the same Borealis localization system as Moonlight-Switch.

| Language | Artemis-specific UI |
|---|---|
| English | ✅ |
| Français | ✅ |
| Other upstream Moonlight-Switch languages | ↩️ Existing/fallback strings until Artemis keys are translated |

French currently covers the Artemis settings page, presentation/motion options, overlay tabs, Performance controls and benchmark actions.

## Continuous Integration

| Gate | Purpose |
|---|---|
| **Unit Tests** | Portable C++20 policy, controller topology, host-key shortcut, statistics, export, stream-profile and geometry tests |
| **ASan + UBSan** | Memory and undefined-behavior checks for the portable feature suite |
| **Localization contract** | Every Artemis key referenced by C++/XML must exist in both English and French |
| **Release contract** | CI verifies the CD workflow still requires NRO, ELF, source archives and checksums |
| **Release package dry run** | Builds and inspects `.tar.gz` and `.zip` source bundles before any release tag |
| **Feature & Integration CI** | Builds/runs the consolidated unit suite plus cross-feature integration tests |
| **All Builds** | Existing platform build matrix, including the real Nintendo Switch `.nro` / `.elf` build |
| **Release CD** | Re-runs quality gates and Switch build before publishing a `v*` GitHub Release |

Additional regression coverage includes the five-controller `0x1F` mask, controller-count clamping, benchmark counter reset/NaN/queue-fault behavior, 4:3 Fit/Fill presentation, filtered Fill/Stretch routing, extreme Zoom/Pan clamping, and minimum/maximum stream-profile normalization.

Moonlight/GameStream negotiates bitrate during setup rather than exposing a safe in-band encoder update. Artemis saves bitrate changes for the next stream connection and does not restart an active stream.

## Automatic releases

`.github/workflows/release.yml` implements GitHub release CD.

### Dry run

Use **Actions → Release Artemis Switch → Run workflow** on a branch. A manual run executes the release quality gate, creates the real Switch NRO/ELF artifacts, and validates source packaging, but it **does not publish a GitHub Release**.

### Publish a release

After the desired commit is merged and validated, create and push a version tag such as:

```bash
git tag v0.1.0
git push origin v0.1.0
```

The tag workflow will only publish after Feature & Integration CI and the Nintendo Switch build succeed.

Every tagged release receives:

| Asset | Purpose |
|---|---|
| `Artemis-Switch.nro` | Installable Nintendo Switch homebrew binary |
| `Artemis-Switch.elf` | Debug/symbol build artifact |
| `Artemis-Switch-X.Y.Z-source.tar.gz` | Source bundle including checked-out submodule contents |
| `Artemis-Switch-X.Y.Z-source.zip` | ZIP variant of the source bundle |
| `SHA256SUMS.txt` | SHA-256 checksums for all release artifacts |
| GitHub auto-generated source archives | Standard GitHub tag source links |

Tags containing a suffix such as `v0.1.0-beta.1` are automatically published as **pre-releases**. Re-running a tag workflow refreshes assets with `--clobber` instead of creating duplicate releases.

## Installation

For the easiest local install, extract `dist/nro/Artemis-Switch-SD.zip` directly to the root of the Switch SD card. It creates:

```text
sdmc:/switch/Artemis-Switch/Artemis-Switch.nro
```

Alternatively, copy `dist/nro/Artemis-Switch.nro` to that path. The same instructions are included in `dist/nro/README-INSTALL.txt`. For published versions, the NRO is also available from GitHub Releases.

Launch hbmenu using **Title Redirection / full RAM mode**, then start Artemis Switch.

## Build

Clone with submodules:

```bash
git clone --recursive https://github.com/antoinebou12/Artemis-Switch.git
cd Artemis-Switch
```

### Nintendo Switch build

Use a standard [devkitPro](https://devkitpro.org/wiki/Getting_Started) Switch development environment:

```bash
cmake -B build/switch -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON
cmake --build build/switch --target Moonlight.nro --parallel
```

CI copies the upstream build outputs to the release-facing names `Artemis-Switch.nro` and `Artemis-Switch.elf`.

### Portable tests

```bash
python tests/i18n_consistency_test.py
python tests/release_contract_test.py
cmake -S tests -B build/tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests --parallel
ctest --test-dir build/tests --output-on-failure
```

### Cross-feature integration tests

```bash
cmake -S ci/integration -B build/integration -DCMAKE_BUILD_TYPE=Debug
cmake --build build/integration --parallel
ctest --test-dir build/integration --output-on-failure
```

## Safety and clocks

Artemis benchmark metadata performs **read-only** clock queries. It does not set CPU, GPU or EMC/RAM clocks. If the relevant service is unavailable, clock metadata remains unavailable/zero rather than modifying the system or failing the benchmark.

Overclocking is outside Artemis Switch's scope.

## Known limitations before the first release

- Multiple simultaneous controllers need multiplayer confirmation against each supported host implementation; routing and topology are unit/build tested.
- Fit, Zoom/Pan and full-range output still benefit from visual verification across different host resolutions.
- Console-motion fallback remains disabled until the libnx console sensor vectors are mapped safely.
- Apollo virtual-display, server-command and clipboard operations are not enabled until their client protocol behavior is verified.
- Benchmark scoring still needs tuning from real Switch measurements.

See [`docs/ARTEMIS_SWITCH_INTEGRATION_PLAN.md`](docs/ARTEMIS_SWITCH_INTEGRATION_PLAN.md) for the detailed release acceptance criteria.

## Upstream and credits

Artemis Switch is based on [XITRIX/Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch), which is part of the wider [Moonlight Game Streaming](https://github.com/moonlight-stream) ecosystem.

Major upstream components remain central to the project: Moonlight common code, Sunshine/GameStream compatibility, Borealis, FFmpeg/NVDEC, deko3d and libnx. Artemis/Apollo behavior is added only where it maps cleanly to the native Switch client.

Thanks to the Moonlight-Switch, Moonlight, Sunshine, Artemis, Apollo, Borealis, FFmpeg, deko3d and libnx contributors.

## License

This fork follows the licensing requirements of Moonlight-Switch and the upstream components it incorporates. See [`LICENSE`](LICENSE) and the relevant source-file notices for details.
