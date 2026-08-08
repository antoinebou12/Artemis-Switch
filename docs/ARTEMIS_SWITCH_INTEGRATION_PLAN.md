# Artemis Switch integration plan

Artemis Switch is now developed through a single consolidated integration branch and pull request rather than one PR per feature.

## Principles

- Reuse Moonlight-Switch's existing Borealis UI, `MoonlightSession`, input stack, NVDEC and deko3d paths.
- Keep Sunshine/GameStream behavior as the safe default.
- Enable Apollo-only behavior only after positive capability detection.
- Keep portable policy/statistics code independently unit testable.
- Require portable tests, sanitizer tests, cross-feature integration tests and the real Nintendo Switch build before release.

## Consolidated feature set

### Benchmark and diagnostics

- live `MoonlightSession` telemetry sampling
- receive/decode/render FPS and latency metrics
- frame-queue fault counters
- mean, median, P95 and P99 aggregation
- Switch-oriented stability score
- JSON/CSV benchmark export
- Artemis Performance tab in the existing in-game overlay

### Stream tuning

- persistent custom resolution
- exact bitrate configuration
- H.264/HEVC profile validation
- 30/40/60/90/120 FPS policy
- Auto Tune quick and extended benchmark plans
- reconnect/warm-up/benchmark/rank/apply runtime

### UI and Switch controls

- native Artemis Borealis settings tab
- existing in-game overlay reused for Quick Actions
- keyboard, performance, benchmark, disconnect and quit-host actions
- Fit / Fill / Stretch state and geometry
- Zoom/Pan persistence
- Joy-Con / Pro Controller motion policy

### Host capabilities

- Sunshine-safe capability fallback
- conservative Apollo detection
- virtual-display/server-command/clipboard/input-only capability flags

## CI

The consolidated PR uses:

1. `.github/workflows/unit-tests.yml` for portable C++ tests and sanitizers.
2. `.github/workflows/feature-integration-ci.yml` for consolidated unit and cross-feature integration suites.
3. `.github/workflows/all-builds.yml` as the real platform build matrix, including Nintendo Switch `.nro`/`.elf` validation.

## Remaining deep integration work

The consolidated feature foundations are present, but these areas still need final platform wiring and real-device validation before release:

- apply Fit / Fill / Stretch to the deko3d presentation viewport
- apply Zoom/Pan to the deko3d UV/presentation transform
- apply the experimental full-range preference in the Switch renderer
- gate the existing controller gyro/accelerometer callback with the persisted motion policy
- map and implement verified Apollo virtual-display/server-command/clipboard operations
- run benchmark/Auto Tune validation on real Switch hardware and tune scoring thresholds from collected data

## Release acceptance

A release candidate should not be tagged until:

- portable unit tests pass
- ASan/UBSan tests pass
- cross-feature integration tests pass
- Nintendo Switch build succeeds
- the `.nro` boots through full-RAM hbmenu/title redirection
- a real stream can start, stop, reconnect and exit safely
- benchmark collection/export is validated on-device
- Auto Tune can cancel and restore the original profile safely
- Sunshine continues to work when Apollo features are unavailable
