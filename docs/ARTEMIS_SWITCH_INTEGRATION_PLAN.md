# Artemis Switch integration plan

This document defines how the open Artemis-Switch feature PRs move from portable models into the real Moonlight-Switch application while keeping the fork Switch-first and reviewable.

## Principles

- Reuse the existing Borealis settings tab and in-game overlay instead of adding a parallel UI framework.
- Reuse `MoonlightSession::session_stats()` and `AVFrameHolder` telemetry instead of adding duplicate instrumentation.
- Reuse Moonlight-Switch controller, Joy-Con motion, keyboard, mouse-mode, and overlay input paths.
- Keep Apollo extensions capability-gated so normal Sunshine/GameStream hosts continue to work.
- Put Switch renderer behavior in the deko3d path and keep portable math independently unit tested.
- Every feature must pass portable unit tests and the real Nintendo Switch build before merge.

## PR responsibilities

1. `agent/test-infrastructure`: portable C++20 tests and common CI helpers.
2. `agent/benchmark-core`: live Moonlight telemetry adapter, benchmark lifecycle, aggregation.
3. `agent/auto-tune`: benchmark profile plan, state machine, recommendation selection.
4. `agent/custom-stream-settings`: persistent custom resolution/bitrate and existing Settings/Borealis UI integration.
5. `agent/scaling-modes`: Fit/Fill/Stretch portable math plus deko3d renderer integration.
6. `agent/apollo-capabilities`: detect and gate Apollo-only capabilities, preserving Sunshine fallback.
7. `agent/benchmark-export`: safe JSON/CSV serialization and Switch filesystem persistence.
8. `agent/performance-lite`: existing `StreamingView` stats overlay gains Artemis-style Lite mode.
9. `agent/advanced-stream-options`: high-FPS policy, full-range-video request, packet-loss policy hooks.
10. `agent/switch-motion-options`: settings around the existing Joy-Con/Pro Controller IMU forwarding path.
11. `agent/quick-stream-menu`: extend the existing `IngameOverlay` with benchmark/performance/keyboard/mouse actions.
12. `agent/zoom-pan`: renderer-facing zoom/pan state, persistence, and Switch controls.
13. `agent/ci-feature-integration`: feature matrix, combined portable integration suite, and full Switch build gate.

## Integration order

### Milestone A: live benchmark

`test-infrastructure -> benchmark-core -> performance-lite -> benchmark-export`

Acceptance: a running Switch stream can start/stop a benchmark, display live stats using the existing overlay, and save a result.

### Milestone B: tuning

`custom-stream-settings -> advanced-stream-options -> auto-tune`

Acceptance: the client can apply tested stream profiles, restart the session safely, collect benchmark results, and recommend a profile.

### Milestone C: rendering and input

`scaling-modes -> zoom-pan -> switch-motion-options -> quick-stream-menu`

Acceptance: Fit/Fill/Stretch and Zoom/Pan affect the deko3d presentation path, motion options control the existing IMU forwarding policy, and the existing in-game overlay exposes the features.

### Milestone D: Apollo

`apollo-capabilities -> Apollo virtual-display/commands follow-up`

Acceptance: Sunshine remains fully functional; Apollo-specific features are enabled only after a positive capability check.

## Shared integration points

- `app/src/utils/Settings.hpp` and `Settings.cpp`: persistent feature settings.
- `app/src/settings_tab.cpp` and `resources/xml/tabs/settings.xml`: existing global settings UI.
- `app/src/streaming/MoonlightSession.*`: stream configuration and live telemetry.
- `app/src/streaming_view.cpp`: live stats/benchmark display.
- `app/src/ingame_overlay_view.cpp` and existing overlay XML: quick in-stream controls.
- `app/src/streaming/video/deko3d/DKVideoRenderer.cpp`: Switch-only scaling/zoom presentation.
- existing `InputManager` paths: Joy-Con/controller motion policy.

## CI acceptance

For each PR:

1. Portable C++ unit tests pass.
2. Feature branch is included in the feature matrix.
3. Combined portable integration suite passes.
4. Existing `All Builds` workflow produces a Nintendo Switch build successfully.
5. Feature code used by the real app is compiled by the main CMake target, not only by the portable test target.
