# Artemis Switch

[![Unit tests](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/unit-tests.yml/badge.svg)](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/unit-tests.yml)
[![Feature & Integration CI](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/feature-integration-ci.yml/badge.svg)](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/feature-integration-ci.yml)
[![All Builds](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/all-builds.yml/badge.svg)](https://github.com/antoinebou12/Artemis-Switch/actions/workflows/all-builds.yml)

**Artemis Switch** is a Nintendo Switch-focused game-streaming client built on top of [Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch), with Artemis/Apollo-inspired stream tuning, benchmarking, diagnostics, and host capabilities.

The goal is to keep the native Horizon OS experience while adding the useful parts of Artemis that map well to Switch hardware and Moonlight's existing C++/Borealis/deko3d architecture.

> [!IMPORTANT]
> Artemis Switch is currently under active development. The open integration PR contains work-in-progress features and should be treated as experimental until the Nintendo Switch build and integration CI are green.

## Highlights

- Native Horizon OS / Atmosphere application
- Moonlight GameStream protocol compatibility
- Sunshine compatibility remains the default path
- Apollo capability detection and extension layer
- Live Switch performance telemetry
- Benchmark mode with P50/P95/P99 statistics
- JSON and CSV benchmark export
- Switch-focused Auto Tune benchmark runner
- Custom resolution and exact bitrate controls
- 30 / 40 / 60 / 90 / 120 FPS policy
- H.264 and HEVC support
- Fit / Fill / Stretch presentation modes
- Zoom and Pan state and persistence
- Joy-Con / Pro Controller gyro and accelerometer policy
- Native Borealis Performance tab
- Quick Actions integrated into the existing in-game overlay
- Portable unit tests, sanitizer CI, cross-feature integration CI, and real Switch build validation

## Current architecture

```text
Apollo / Sunshine host
        |
        | GameStream + optional Apollo extensions
        v
Artemis Switch
  |
  +-- MoonlightSession
  |     +-- live telemetry
  |     +-- custom stream configuration
  |
  +-- Benchmark Runtime
  |     +-- receive/decode/render metrics
  |     +-- frame queue metrics
  |     +-- P50/P95/P99
  |     +-- stability score
  |     +-- JSON/CSV export
  |
  +-- Auto Tune
  |     +-- apply profile
  |     +-- reconnect
  |     +-- warm up
  |     +-- benchmark
  |     +-- rank
  |     +-- apply winner
  |
  +-- Borealis UI
  |     +-- Artemis settings tab
  |     +-- Performance tab
  |     +-- Quick Actions
  |
  +-- deko3d / NVDEC
        +-- Switch video decode/render path
```

## Planned Auto Tune flow

```text
720p60 HEVC / 15 Mbps
        |
        v
1080p60 / 15 Mbps
        |
        v
1080p60 / 20 Mbps
        |
        v
1080p60 / 25 Mbps
        |
        v
1080p60 / 30 Mbps
        |
        v
compare stability, loss, FPS and P99 latency
        |
        v
apply best stable profile
```

## Installation on Nintendo Switch

A release package is not finalized yet. Development builds use the same homebrew layout as Moonlight-Switch.

1. Copy the generated `.nro` to your SD card, for example:

   ```text
   sdmc:/switch/Artemis-Switch/Artemis-Switch.nro
   ```

2. Launch hbmenu using **Title Redirection / full RAM mode**.
3. Start Artemis Switch.

For high-resolution, high-bitrate streaming, performance can depend on the Switch model, clock configuration, decoder settings, and network quality. Do not assume higher clocks or bitrate automatically produce a better result; the benchmark and Auto Tune work is intended to measure this directly.

## Build

Clone with submodules:

```bash
git clone --recursive https://github.com/antoinebou12/Artemis-Switch.git
cd Artemis-Switch
```

### Nintendo Switch

Install a standard [devkitPro](https://devkitpro.org/wiki/Getting_Started) Switch development environment, then:

```bash
cmake -B build/switch -DPLATFORM_SWITCH=ON
cmake --build build/switch --target Moonlight.nro --parallel
```

The project currently inherits the upstream Moonlight-Switch target/file naming while the Artemis branding migration is completed.

### Portable feature tests

```bash
cmake -S tests -B build/tests
cmake --build build/tests --parallel
ctest --test-dir build/tests --output-on-failure
```

## Continuous Integration

Artemis Switch uses three main GitHub Actions gates:

- **Unit tests:** portable C++20 feature tests plus sanitizer coverage.
- **Feature & Integration CI:** validates feature branches independently and builds a combined feature tree for cross-feature tests.
- **All Builds:** the inherited platform build matrix, including the Nintendo Switch `.nro` / `.elf` build.

The badges at the top of this README show the current state of each gate.

## Development status

The project is being consolidated into a single integration PR. The main remaining deep integration work is concentrated in:

- deko3d Fit / Fill / Stretch and Zoom/Pan presentation transforms
- full-range renderer application
- gating the existing Joy-Con motion callback with the new policy
- verified Apollo virtual-display / server-command protocol integration

See [`docs/ARTEMIS_SWITCH_INTEGRATION_PLAN.md`](docs/ARTEMIS_SWITCH_INTEGRATION_PLAN.md) for the detailed implementation and merge plan.

## Upstream

Artemis Switch is a fork of [XITRIX/Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch), which is itself based on the [Moonlight Game Streaming](https://github.com/moonlight-stream) ecosystem.

The project continues to use major upstream components including Moonlight common code, Borealis, FFmpeg/NVDEC and deko3d integrations. Changes should preserve Sunshine compatibility unless an Apollo-specific capability has been positively detected.

## Credits

Thanks to the Moonlight-Switch, Moonlight, Sunshine, Artemis and Apollo contributors, as well as the developers of Borealis, FFmpeg/NVDEC, deko3d and the other upstream dependencies this fork builds upon.

Original Moonlight-Switch credits and license history remain in the repository and Git history.

## License

This fork follows the licensing requirements of Moonlight-Switch and the upstream components it incorporates. See [`LICENSE`](LICENSE) and the relevant source-file notices for details.
