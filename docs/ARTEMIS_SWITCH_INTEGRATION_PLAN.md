# Artemis Switch integration plan

Artemis Switch is developed through a single consolidated integration branch and pull request rather than one PR per feature.

## Principles

- Reuse Moonlight-Switch's existing Borealis UI, `MoonlightSession`, input stack, NVDEC and deko3d paths.
- Keep Sunshine/GameStream behavior as the safe default.
- Enable Apollo-only behavior only after positive capability detection.
- Keep portable policy/statistics and presentation geometry independently unit testable.
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
- benchmark runtime metadata for handheld/docked mode, battery level, charging-enabled state and best-effort read-only CPU/GPU/EMC clock rates

### Stream tuning

- persistent custom resolution
- exact bitrate configuration
- H.264/HEVC profile validation
- 30/40/60/90/120 FPS policy
- Auto Tune quick and extended benchmark plans
- reconnect/warm-up/benchmark/rank/apply runtime
- full-range preference applied both to Moonlight `STREAM_CONFIGURATION.colorRange` and Switch renderer conversion

### UI and Switch controls

- native Artemis Borealis settings tab
- existing in-game overlay reused for Quick Actions
- keyboard, performance, benchmark, disconnect and quit-host actions
- Fit / Fill / Stretch state and tested presentation geometry
- Zoom/Pan persistence and tested source-crop geometry
- Joy-Con / Pro Controller motion policy
- console-motion fallback is shown as unavailable until the runtime exposes a distinct console IMU source

### Nintendo Switch runtime integration

- existing Moonlight-Switch deko3D renderer preserved as the compatibility implementation
- normal `Fill` presentation continues through the existing renderer, including its existing NVDEC, FSR/RCAS/dithering and frame-stat paths
- `Fit`, `Stretch`, Zoom/Pan and forced full-range video are connected to an Artemis deko3D direct-presentation path
- Fit mode clears the full framebuffer and renders into a centered aspect-preserving viewport for black letterbox/pillarbox bars
- Fill and Zoom/Pan use source UV cropping rather than resizing decoded frames on the CPU
- forced full range requests `COLOR_RANGE_FULL` from the host and reuses the existing deko3D YUV full-range conversion matrices
- the existing `LiSendControllerMotionEvent()` boundary is policy-gated without rewriting Moonlight-Switch's input implementation
- console-IMU fallback remains capability-gated because the current Borealis controller sensor callback does not identify a distinct console motion source
- Switch benchmark metadata uses read-only libnx services and never changes clocks

The first Artemis presentation milestone intentionally uses the direct deko3D path whenever Fit, Stretch, Zoom/Pan or forced full range is active. The untouched default Fill path retains the current FSR/RCAS/dithering pipeline. After real-device validation, the same presentation geometry can be propagated through the post-processing path so those effects remain available in every scale mode.

### Host capabilities

- Sunshine-safe capability fallback
- conservative Apollo detection
- virtual-display/server-command/clipboard/input-only capability flags

## CI

The consolidated PR uses:

1. `.github/workflows/unit-tests.yml` for portable C++ tests and sanitizers.
2. `.github/workflows/feature-integration-ci.yml` for consolidated unit and cross-feature integration suites.
3. `.github/workflows/all-builds.yml` as the real platform build matrix, including Nintendo Switch `.nro`/`.elf` validation.

## Remaining deep integration and validation work

The main Switch presentation, motion, full-range negotiation and benchmark metadata hooks are now implemented in code. Remaining release work is:

- obtain a green Nintendo Switch `.nro` / `.elf` build for the consolidated PR
- boot and stream-test Fit / Fill / Stretch on real Switch hardware
- validate Zoom/Pan direction, edges and persistence on-device
- validate forced full-range output against known limited/full-range host content
- verify that read-only CPU/GPU/EMC clock queries are permitted on the target homebrew environment; unavailable services intentionally export zero
- propagate Artemis presentation geometry through the existing FSR/RCAS/dithering post-processing path after the direct path is proven stable
- expose a distinct console-IMU fallback only if Borealis/libnx provides an unambiguous source separate from controller motion
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
- Fit / Fill / Stretch and Zoom/Pan are visually verified on the Switch
- benchmark collection/export is validated on-device
- Auto Tune can cancel and restore the original profile safely
- Sunshine continues to work when Apollo features are unavailable
