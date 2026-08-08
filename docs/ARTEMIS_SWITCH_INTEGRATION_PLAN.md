# Artemis Switch integration plan

This document defines how the open Artemis-Switch feature PRs move from portable models into the real Moonlight-Switch application while keeping the fork Switch-first and reviewable.

## Principles

- Reuse the existing Borealis settings tab and in-game overlay instead of adding a parallel UI framework.
- Reuse `MoonlightSession::session_stats()` and `AVFrameHolder` telemetry instead of adding duplicate instrumentation.
- Reuse Moonlight-Switch controller, Joy-Con motion, keyboard, mouse-mode, and overlay input paths.
- Keep Apollo extensions capability-gated so normal Sunshine/GameStream hosts continue to work.
- Put Switch renderer behavior in the deko3d path and keep portable math independently unit tested.
- Every feature must pass portable unit tests and the real Nintendo Switch build before merge.

## PR responsibilities and current status

1. `agent/test-infrastructure`: portable C++20 tests, GitHub Actions, ASan and UBSan coverage.
2. `agent/benchmark-core`: benchmark aggregation plus autonomous 250 ms sampling from live `MoonlightSession` and `AVFrameHolder` telemetry.
3. `agent/auto-tune`: quick/extended Switch test plans, state machine and asynchronous profile/restart/benchmark/recommend loop. Automatically cooperates with custom-resolution settings when present.
4. `agent/custom-stream-settings`: persistent custom resolution and exact bitrate, native Borealis Artemis settings tab, real `MoonlightSession::start()` negotiation. The tab conditionally exposes compatible options from PRs 5, 9, 10 and 12 when combined.
5. `agent/scaling-modes`: tested Fit/Fill/Stretch geometry and persistent mode. Borealis setting is exposed through PR 4 when combined. Final deko3d viewport application is still pending.
6. `agent/apollo-capabilities`: conservative Apollo/Sunshine capability detection plus adapter from real cached `SERVER_DATA`. Apollo-specific HTTP/protocol operations remain a follow-up after endpoint compatibility is mapped.
7. `agent/benchmark-export`: escaped JSON/CSV serialization plus safe filesystem persistence to benchmark files.
8. `agent/performance-lite`: real Performance tab in the existing in-game Borealis overlay. Uses live session/queue telemetry and conditionally exposes benchmark, export and Auto Tune controls when those PRs are present.
9. `agent/advanced-stream-options`: tested 30/40/60 versus 90/120 FPS policy plus persistent full-range and packet-loss experimental preferences. Packet-loss transport mutation is intentionally not enabled until Artemis Android behavior is verified. Full-range renderer application is pending.
10. `agent/switch-motion-options`: tested Joy-Con/Pro/console motion policy plus persistence. The low-level Moonlight-Switch motion forwarding already exists; gating the large existing `InputManager.cpp` callback is pending a safe targeted patch.
11. `agent/quick-stream-menu`: real Quick Actions section added to the existing in-stream Options overlay: keyboard, performance stats, conditional benchmark, disconnect and quit-host.
12. `agent/zoom-pan`: tested normalized zoom/pan state plus persistence/reset/remember policy. Borealis setting is exposed through PR 4 when combined. Final deko3d transform and in-stream pan/zoom controls are pending.
13. `agent/ci-feature-integration`: feature matrix, combined portable integration suite, implementation plan and full Switch build gate.

## Integration order

### Milestone A: live benchmark

`test-infrastructure -> benchmark-core -> performance-lite -> benchmark-export`

Acceptance: a running Switch stream can start/stop a benchmark, display live stats using the existing overlay, and save JSON/CSV results.

### Milestone B: tuning

`custom-stream-settings -> advanced-stream-options -> auto-tune`

Acceptance: the client can apply tested stream profiles, restart the session safely, collect benchmark results, and recommend/apply a profile.

### Milestone C: rendering and input

`scaling-modes -> zoom-pan -> switch-motion-options -> quick-stream-menu`

Acceptance: Fit/Fill/Stretch and Zoom/Pan affect the deko3d presentation path, motion options gate the existing IMU forwarding policy, and the existing in-game overlay exposes the features.

### Milestone D: Apollo

`apollo-capabilities -> Apollo virtual-display/commands follow-up`

Acceptance: Sunshine remains fully functional; Apollo-specific features are enabled only after a positive capability check.

## Shared integration points

- `app/src/utils/Settings.hpp` and `Settings.cpp`: existing Moonlight settings.
- `app/src/settings_tab.cpp`: existing standard settings UI, kept intact where possible.
- `app/src/artemis_settings_tab.cpp`: Artemis-specific tuning UI and optional feature composition.
- `app/src/streaming/MoonlightSession.*`: stream configuration and live telemetry.
- `app/src/streaming_view.cpp`: existing advanced stats rendering.
- existing `IngameOverlay`, Options XML and Performance tab: in-stream controls.
- `app/src/streaming/video/deko3d/DKVideoRenderer.cpp`: Switch-only scaling/zoom/full-range presentation hook.
- existing `InputManager` paths: Joy-Con/controller motion policy hook.

## CI acceptance

For each PR:

1. Portable C++ unit tests pass.
2. Sanitizer tests pass where the portable harness is present.
3. Feature branch is included in the feature matrix.
4. Combined portable integration suite passes.
5. Existing `All Builds` workflow produces a Nintendo Switch build successfully.
6. Feature code used by the real app is compiled by the main CMake target, not only by the portable test target.

Large platform source files are not replaced wholesale merely to land a feature. Renderer/input changes must be targeted and validated by the real Switch build to minimize regressions.
