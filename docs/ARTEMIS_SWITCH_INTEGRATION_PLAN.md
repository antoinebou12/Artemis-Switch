# artemi-switch integration plan

`artemi-switch` keeps Moonlight-Switch's proven streaming stack and adds Switch-focused presentation, performance, and carefully gated Apollo integration. New controls are exposed only when their runtime path is real and testable.

## UI placement rules

### Quick Actions: essentials only

Quick Actions are intentionally short and session-focused:

- keyboard
- performance overlay
- pointer/mouse mode
- disconnect
- quit host app when supported

Benchmark/test controls, image-filter tuning, rotation, virtual display, and other advanced configuration do **not** belong in Quick Actions.

### Options: live and diagnostic controls

The in-stream Options page owns controls that are useful while a session is active:

- input and keyboard behavior
- pointer/mouse behavior
- audio and rumble
- image filtering such as FSR, RCAS, and dithering
- rotation once the renderer/input transform backend is complete
- benchmark/test and debug controls under a dedicated Diagnostics section

### Settings: persistent stream configuration

The main settings page is grouped by responsibility:

1. **Stream profile**: resolution, exact bitrate, FPS, codec/decoder information.
2. **Presentation**: Fit/Fill/Stretch, Full Range, filter status, Zoom/Pan persistence.
3. **Advanced network**: experimental transport policies only.
4. **Motion**: controller motion and safe console fallback policy.

The FPS selector always presents 30/40/60/90/120 FPS. There is no second “unlock high FPS” toggle.

### Pre-launch setup: Apollo virtual display

Virtual display is configured **before launching an app**, not from the in-stream Quick menu. The implementation must:

- appear only after positive Apollo virtual-display capability detection
- default to Off
- offer current profile, handheld, docked, portrait, and validated custom targets
- check driver readiness before launch
- add Apollo-only launch parameters only for Apollo hosts
- leave Sunshine launch requests unchanged
- use controlled reconnect + rollback for an active resolution change

Until the Apollo launch/query backend is implemented and tested, no non-functional virtual-display control should be shown.

## Renderer and video filtering

Fit, Fill, Stretch, Zoom/Pan, and Full Range use one deko3d presentation path. Switching scale modes must not jump between unrelated renderer implementations.

The unified path reuses Moonlight-Switch's existing:

- NVDEC/NVTEGRA frame mappings
- deko3d descriptor and command infrastructure
- FSR/EASU resources
- RCAS sharpening
- dithering
- renderer statistics

Presentation behavior:

- **Fit**: preserve aspect ratio and clear unused framebuffer regions to black.
- **Fill**: crop source UVs to fill the output without CPU frame resizing.
- **Stretch**: present the complete source over the full output viewport.
- **Zoom/Pan**: crop the selected source region on the GPU.
- **Full Range**: request `COLOR_RANGE_FULL` from the host and use full-range YUV conversion without disabling video filtering.

Filter/resource allocation failure falls back to the same presentation geometry on the direct path rather than silently changing the selected scale mode.

## Performance and diagnostics

The Performance page should prioritize actionable live information without repeating the same profile in multiple rows. It includes:

- configured resolution/FPS/codec
- receive and decode latency
- packet loss
- rendered FPS
- frame render time
- post-process time
- FSR time
- RCAS time
- dithering time
- GPU render time when available
- frame queue current/target/capacity
- active Fit/Fill/Stretch mode
- Full/Limited range state
- benchmark and Auto Tune controls

Benchmark/test actions remain under Performance/Diagnostics rather than Quick Actions.

## Rotation

Rotation belongs in Options, not Quick Actions. 0°, 90°, 180°, and 270° must share a single transform used by rendering, absolute input, relative vectors, cursor placement, Zoom/Pan, and virtual-control hit testing. Do not expose the selector until those mappings are implemented together.

## Apollo capability work

Apollo support remains capability-gated. Required backend work includes:

- parse real server-info virtual-display capability/readiness fields
- preserve app UUIDs and server ordering
- virtual-display launch/resume parameters
- Remote Input UUID handling
- server commands
- plain-text clipboard operations
- permission-aware error handling

Sunshine must receive no Apollo-only parameters.

## CI and release acceptance

Before release:

- portable unit tests pass
- ASan/UBSan pass
- localization contracts pass
- cross-feature integration tests pass
- Nintendo Switch `.nro` / `.elf` build succeeds
- Fit/Fill/Stretch are visually verified on hardware
- Full Range is checked with known limited/full-range content
- FSR/RCAS/dithering remain active in every presentation mode when enabled
- scale changes do not flicker or expose stale framebuffer content
- 90/120 FPS remain explicit profile choices
- Sunshine streaming remains unchanged when Apollo features are unavailable
- Apollo virtual display is not exposed until launch/readiness/rollback behavior is verified
