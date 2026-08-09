# Artemis Switch

[![Unit Tests](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/unit-tests.yml/badge.svg?branch=agent%2Fartemis-switch-integration)](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/unit-tests.yml)
[![Feature & Integration CI](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/feature-integration-ci.yml/badge.svg?branch=agent%2Fartemis-switch-integration)](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/feature-integration-ci.yml)
[![All Builds](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/all-builds.yml/badge.svg?branch=agent%2Fartemis-switch-integration)](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/all-builds.yml)
[![Release CD](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/release.yml/badge.svg)](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/release.yml)
[![Latest Release](https://img.shields.io/github/v/release/antoinebou12/Artemis-Switch?display_name=tag&sort=semver)](https://github.com/antoinebou12/Artemis-Switch/releases)
[![Platform](https://img.shields.io/badge/platform-Nintendo%20Switch-E60012)](#nintendo-switch)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C)](CMakeLists.txt)
[![Languages](https://img.shields.io/badge/UI-English%20%7C%20Fran%C3%A7ais-4c8bf5)](#localization)
[![Status](https://img.shields.io/badge/status-experimental-orange)](#project-status)

**Artemis Switch** is a native Horizon OS game-streaming client for Nintendo Switch, built on [Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch) and focused on Switch-specific streaming diagnostics, tuning, presentation controls, and carefully gated Apollo integration.

It keeps the existing Moonlight-Switch architecture instead of replacing it: **Borealis UI, Moonlight/GameStream, FFmpeg/NVDEC, deko3d, Joy-Con input, Sunshine compatibility, and the existing post-processing path remain the foundation.**

> [!IMPORTANT]
> Artemis Switch is under active development. PR #15 contains the current consolidated implementation. Features marked **Device validation** are implemented in code but still need a green Nintendo Switch build and real-hardware verification before the first stable release is tagged.

## Project status

| Area | Status | Current state |
|---|---|---|
| Standard Moonlight / Sunshine streaming | ✅ Available | Standard GameStream remains the compatibility path |
| Switch NVDEC + deko3d | 🟡 Device validation | Existing renderer preserved; Artemis presentation path integrated |
| Fit / Fill / Stretch | 🟡 Device validation | GPU viewport / UV presentation implemented |
| Zoom & Pan | 🟡 Device validation | GPU source crop with persistent state |
| Full-range video | 🟡 Device validation | Requested from host and applied with deko3d full-range YUV conversion |
| Joy-Con / Pro Controller motion | ✅ Integrated | Existing Moonlight motion forwarding is policy-gated |
| Console-motion fallback | ⛔ Disabled by default | libnx SevenSixAxis API can be detected, but console motion vectors are not mapped safely yet |
| Live performance view | ✅ Integrated | Uses the existing in-game Borealis overlay |
| Benchmark runtime | ✅ Integrated | Live sampling, P50/P95/P99, frame-queue faults, stability score |
| Benchmark JSON / CSV | ✅ Integrated | Includes Switch runtime metadata when services are available |
| Auto Tune | 🟡 Device validation | Reconnect → warm-up → benchmark → rank → apply winner |
| Apollo capability detection | 🟡 Partial | Conservative detection with Sunshine-safe fallback |
| Apollo virtual display / commands / clipboard | ⏳ Planned | Only verified Apollo protocol operations will be added |
| French Artemis UI | ✅ Integrated | Artemis settings, overlay tabs and Performance UI use Borealis i18n |
| CI | 🟡 In progress | Unit, sanitizer, localization, release-contract and cross-feature gates are defined |
| Release CD | ✅ Integrated | `v*` tags build/test/publish NRO, ELF, source bundles and SHA-256 checksums |

## Features

| Feature | Artemis Switch implementation |
|---|---|
| Custom resolution | Persistent width / height override used by `MoonlightSession` |
| Exact bitrate | 1–100 Mbps configuration using the existing Moonlight settings path |
| Frame rates | 30 / 40 / 60 FPS plus gated 90 / 120 FPS options |
| Video codecs | H.264 and HEVC, using the existing decoder path |
| Presentation | Fit, Fill, Stretch, Zoom and Pan on the Switch deko3d path |
| Full range | Host color-range negotiation plus Switch-side deko3d conversion |
| Motion | Joy-Con / controller gyro and accelerometer forwarding policy |
| Performance | Receive, decode, render, FPS, packet-loss and queue telemetry |
| Benchmark | Mean, median, P95, P99 and stability scoring |
| Export | JSON + CSV files saved under the Artemis working directory |
| Switch metadata | Handheld/docked mode, battery, charging state and best-effort read-only CPU/GPU/EMC clocks |
| Auto Tune | Quick and extended profile sweeps with safe profile restore/cancel logic |
| Quick Actions | Keyboard, performance, benchmark, disconnect and quit-host actions in the existing overlay |
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
│   ├── stream configuration / reconnect                         │
│   ├── custom resolution / bitrate / FPS                        │
│   └── live session telemetry                                   │
│                                                               │
│ Benchmark + Auto Tune                                          │
│   ├── receive / decode / render / queue metrics                │
│   ├── P50 / P95 / P99 + stability                             │
│   ├── JSON / CSV + Switch metadata                             │
│   └── profile sweep / rank / apply                            │
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

The normal compatibility **Fill** path keeps the existing Moonlight-Switch renderer behavior, including its NVDEC mappings and current FSR/RCAS/dithering pipeline.

When Artemis presentation features are requested, the Switch renderer can use the tested presentation geometry directly:

- **Fit:** preserve aspect ratio and clear unused framebuffer regions to black.
- **Fill:** crop source UVs to fill the destination without CPU frame resizing.
- **Stretch:** use the full destination viewport.
- **Zoom/Pan:** apply bounded GPU-side source cropping.
- **Full range:** request full-range video from the host and use the corresponding deko3d YUV matrix.

The first custom-presentation milestone intentionally bypasses FSR/RCAS/dithering outside the normal Fill path. After real-device validation, the same presentation geometry can be carried through the post-processing present pass.

### Motion sensors

Joy-Con and compatible controller motion continues to use Moonlight-Switch's existing controller-motion path.

**Console-motion fallback is OFF by default.** Artemis can detect whether the libnx SevenSixAxis API is available, but does not treat opaque/undocumented sensor fields as acceleration or gyro data. The UI therefore remains disabled until the console-motion vectors are mapped and validated unambiguously.

## Benchmark and Auto Tune

A benchmark samples the active stream instead of using synthetic data. Results include frame rate, receive/decode/render timing, packet loss, frame-queue behavior and percentile statistics.

Example Auto Tune flow:

```text
720p60 HEVC / 15 Mbps
        ↓
1080p60 / 15 Mbps
        ↓
1080p60 / 20 Mbps
        ↓
1080p60 / 25 Mbps
        ↓
1080p60 / 30 Mbps
        ↓
compare stability + loss + FPS + P99 latency
        ↓
apply the best stable profile
```

Auto Tune temporarily removes conflicting custom-resolution overrides while testing and restores the previous configuration on cancellation/failure.

## Localization

Artemis-specific UI uses the same Borealis localization system as Moonlight-Switch.

| Language | Artemis-specific UI |
|---|---|
| English | ✅ |
| Français | ✅ |
| Other upstream Moonlight-Switch languages | ↩️ Existing/fallback strings until Artemis keys are translated |

French currently covers the Artemis settings page, presentation/motion options, overlay tabs, Performance controls, benchmark actions and Auto Tune controls.

## Continuous Integration

| Gate | Purpose |
|---|---|
| **Unit Tests** | Portable C++20 policy, statistics, export, stream-profile and geometry tests |
| **ASan + UBSan** | Memory and undefined-behavior checks for the portable feature suite |
| **Localization contract** | Every Artemis key referenced by C++/XML must exist in both English and French |
| **Release contract** | CI verifies the CD workflow still requires NRO, ELF, source archives and checksums |
| **Release package dry run** | Builds and inspects `.tar.gz` and `.zip` source bundles before any release tag |
| **Feature & Integration CI** | Builds/runs the consolidated unit suite plus cross-feature integration tests |
| **All Builds** | Existing platform build matrix, including the real Nintendo Switch `.nro` / `.elf` build |
| **Release CD** | Re-runs quality gates and Switch build before publishing a `v*` GitHub Release |

Additional regression coverage includes benchmark counter reset/NaN/queue-fault behavior, 4:3 Fit/Fill presentation, extreme Zoom/Pan clamping, and minimum/maximum stream-profile normalization.

The development badges at the top target the active `agent/artemis-switch-integration` branch while PR #15 is under development.

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

When a release is available, download `Artemis-Switch.nro` from the GitHub Releases page and place it on the SD card, for example:

```text
sdmc:/switch/Artemis-Switch/Artemis-Switch.nro
```

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

Overclocking is outside Artemis Switch's automatic tuning scope. Auto Tune changes streaming parameters, not console clock rates.

## Known limitations before the first release

- A green Nintendo Switch `.nro` / `.elf` build is required after the latest integration changes.
- Fit / Fill / Stretch, Zoom/Pan and full-range output need visual verification on real Switch hardware.
- Custom presentation still needs to be propagated through the existing FSR/RCAS/dithering post-processing pass after the direct path is validated.
- Console-motion fallback remains disabled until the libnx console sensor vectors are mapped safely.
- Apollo virtual-display, server-command and clipboard operations are not enabled until their client protocol behavior is verified.
- Benchmark scoring and Auto Tune thresholds still need tuning from real Switch measurements.

See [`docs/ARTEMIS_SWITCH_INTEGRATION_PLAN.md`](docs/ARTEMIS_SWITCH_INTEGRATION_PLAN.md) for the detailed release acceptance criteria.

## Upstream and credits

Artemis Switch is based on [XITRIX/Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch), which is part of the wider [Moonlight Game Streaming](https://github.com/moonlight-stream) ecosystem.

Major upstream components remain central to the project: Moonlight common code, Sunshine/GameStream compatibility, Borealis, FFmpeg/NVDEC, deko3d and libnx. Artemis/Apollo behavior is added only where it maps cleanly to the native Switch client.

Thanks to the Moonlight-Switch, Moonlight, Sunshine, Artemis, Apollo, Borealis, FFmpeg, deko3d and libnx contributors.

## License

This fork follows the licensing requirements of Moonlight-Switch and the upstream components it incorporates. See [`LICENSE`](LICENSE) and the relevant source-file notices for details.
