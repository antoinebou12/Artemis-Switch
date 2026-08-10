# artemi-switch

Native Moonlight-compatible game streaming for Nintendo Switch / Horizon OS.

**Author:** `antoinebou12`

`artemi-switch` is based on Moonlight-Switch and keeps its Borealis UI, Moonlight/GameStream transport, FFmpeg/NVDEC decode path, deko3d renderer, controller input, and Sunshine compatibility. The project adds Switch-focused presentation controls, performance diagnostics, stream tuning, and capability-gated Apollo integration.

## Current focus

### Stable presentation path

Fit, Fill, Stretch, Zoom/Pan, and forced Full Range now use one Switch presentation path rather than switching between a filtered legacy path and a separate direct path.

- **Fit:** aspect-preserving output with black letterbox/pillarbox regions.
- **Fill:** source UV crop to cover the output.
- **Stretch:** complete source mapped to the output viewport.
- **Zoom/Pan:** bounded GPU-side source crop.
- **Full Range:** requests `COLOR_RANGE_FULL` and keeps the selected video filtering path active.

The unified path reuses the existing NVDEC/NVTEGRA mappings, deko3d command/resource management, FSR/EASU, RCAS, dithering, and renderer telemetry.

### Stream profiles

The FPS selector always exposes:

- 30 FPS
- 40 FPS
- 60 FPS
- 90 FPS
- 120 FPS

There is no separate high-FPS unlock switch.

### UI layout

**Quick Actions** contain only in-session essentials: keyboard, performance, pointer/mouse mode, disconnect, and optional quit-host.

**Options** contain live input/audio/video controls plus diagnostics and benchmark/test controls. Rotation belongs here once the shared render/input transform backend is complete.

**Settings** are grouped into stream profile, presentation, advanced network, and motion sections. Presentation shows Fit/Fill/Stretch, Full Range, and current FSR/RCAS/dithering state.

**Apollo virtual display** is a pre-launch feature. It will be offered before starting an app after Apollo capability, driver-readiness, launch parameters, reconnect, and rollback behavior are implemented. It is not a Quick Action.

## Performance data

The Performance page includes:

- configured resolution, FPS, and codec
- configured bitrate
- receive latency
- decode latency
- rendered FPS
- packet loss
- frame render time
- post-process time
- FSR time
- RCAS time
- dithering time
- GPU render time when available
- frame queue current / target / capacity
- active Fit / Fill / Stretch mode
- Full / Limited range state
- benchmark and Auto Tune controls

## Build

Clone recursively and build with a standard devkitPro Switch environment:

```bash
git clone --recursive https://github.com/antoinebou12/Artemis-Switch.git artemi-switch
cd artemi-switch
cmake -B build/switch -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON
cmake --build build/switch --target Moonlight.nro --parallel
```

`Moonlight.nro` and `Moonlight.elf` remain internal upstream-compatible build targets. CI exports the user-facing artifacts as:

```text
artemi-switch.nro
artemi-switch.elf
```

### Portable validation

```bash
python tests/i18n_consistency_test.py
python tests/release_contract_test.py
cmake -S tests -B build/tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests --parallel
ctest --test-dir build/tests --output-on-failure
```

## Release packaging

Release source bundles are named:

```text
artemi-switch-X.Y.Z-source.tar.gz
artemi-switch-X.Y.Z-source.zip
```

`SOURCE_INFO.txt` records the project name, author `antoinebou12`, version, commit, and generation time.

## Compatibility policy

Sunshine remains the safe GameStream-compatible path. Apollo-only parameters must never be sent unless Apollo support is positively detected and the specific operation is supported. Non-functional controls are not exposed just to make a menu look complete.

## Remaining runtime work

- validate the unified Fit/Fill/Stretch + FSR/RCAS/dithering path on real Switch hardware
- add one shared 0°/90°/180°/270° render/input transform before exposing rotation
- implement Apollo virtual-display readiness, launch/resume parameters, controlled reconnect, and rollback before exposing the pre-launch selector
- finish Apollo Remote Input, commands, and clipboard only against verified protocol behavior
- tune benchmark/Auto Tune thresholds from real Switch measurements

See `docs/ARTEMIS_SWITCH_INTEGRATION_PLAN.md` for the detailed placement and release rules.

## Credits and license

`artemi-switch` builds on XITRIX/Moonlight-Switch, Moonlight common code, Sunshine/GameStream, Borealis, FFmpeg/NVDEC, deko3d, libnx, and their contributors. Existing upstream license and source-file notice requirements remain applicable.
