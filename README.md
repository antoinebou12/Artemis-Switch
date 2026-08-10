# artemi-switch

Native Moonlight-compatible game streaming for Nintendo Switch / Horizon OS.

**Author:** `antoinebou12`

`artemi-switch` is based on Moonlight-Switch and keeps its proven Borealis UI, Moonlight/GameStream transport, FFmpeg/NVDEC decode path, deko3d renderer, controller input, and Sunshine compatibility. The fork focuses on Switch-specific video presentation, performance visibility, and cleaner stream controls.

## Features

### Stable video presentation

Fit, Fill, Stretch, Zoom/Pan, and Full Range use one Switch presentation path instead of switching between separate filtered and direct renderers.

- **Fit:** preserves aspect ratio with stable black letterbox/pillarbox regions.
- **Fill:** crops source UVs to cover the complete output.
- **Stretch:** maps the complete source to the full output viewport.
- **Zoom/Pan:** performs a bounded GPU-side source crop.
- **Full Range:** requests `COLOR_RANGE_FULL` and keeps the video filtering pipeline available.

The presentation path reuses the existing NVDEC/NVTEGRA frame mappings and deko3d rendering infrastructure.

### Video filtering

The existing Switch GPU filtering stack remains available with the presentation modes:

- FSR/EASU upscaling
- RCAS sharpening
- dithering
- Full/Limited range conversion

FSR is enabled only when its required GPU resources are valid. If filtering resources are unavailable, artemi-switch keeps the selected Fit/Fill/Stretch geometry and falls back safely instead of changing presentation mode.

### Stream profiles

The stream FPS selector always exposes:

- 30 FPS
- 40 FPS
- 60 FPS
- 90 FPS
- 120 FPS

There is no separate high-FPS unlock switch.

Custom resolution and exact bitrate controls are available from the artemi-switch settings page.

### Cleaner controls

**Quick Actions** contain only frequently used in-session controls:

- keyboard
- performance overlay
- pointer/mouse mode
- disconnect
- quit host app when available

**Options** contain live input, audio, image-filtering, benchmark, and diagnostic controls.

**Settings** are grouped into:

- stream profile
- presentation
- advanced stream/network options
- motion

Presentation settings keep Fit/Fill/Stretch, Full Range, Zoom/Pan persistence, and current FSR/RCAS/dithering status together.

### Performance and diagnostics

The Performance page exposes live streaming and renderer information including:

- configured resolution, FPS, and codec
- configured bitrate
- receive latency
- decode latency
- packet loss
- rendered FPS
- frame render time
- post-processing time
- FSR time
- RCAS time
- dithering time
- GPU render time when available
- frame queue current / target / capacity
- active Fit / Fill / Stretch mode
- Full / Limited range state
- benchmark controls
- Auto Tune controls

### Nintendo Switch optimizations

The Switch build keeps the existing hardware decode/render path and also uses:

- optimized WLAN priority mode
- Switch-oriented frame queue telemetry
- Switch motion/controller policy controls
- existing NVDEC hardware decoding
- deko3d GPU rendering and post-processing

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

### Validation

```bash
python tests/i18n_consistency_test.py
python tests/release_contract_test.py
cmake -S tests -B build/tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests --parallel
ctest --test-dir build/tests --output-on-failure
```

CI also contains integration, sanitizer, release-package, and Nintendo Switch build workflows.

## Release packaging

Release source bundles are named:

```text
artemi-switch-X.Y.Z-source.tar.gz
artemi-switch-X.Y.Z-source.zip
```

`SOURCE_INFO.txt` records the project name, author `antoinebou12`, version, commit, and generation time.

## Compatibility

Sunshine and standard Moonlight/GameStream behavior remain the compatibility baseline. Internal upstream build targets and the legacy settings directory are retained where required so existing installations do not lose their saved configuration.

## Credits and license

Artemis Switch is a fork of [Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch) by **XITRIX**. The original Moonlight-Switch source, Borealis UI, Moonlight/GameStream client, FFmpeg/NVDEC decode path, deko3d renderer, and input stack remain the foundation. Existing upstream license and source-file notice requirements remain applicable — see [`LICENSE`](LICENSE) and notices in individual source files.

### Original projects

| Project | Role | Link |
|---|---|---|
| [XITRIX/Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch) | Original Switch client (this fork’s base) | Upstream |
| [ClassicOldSong/Apollo](https://github.com/ClassicOldSong/Apollo) | Sunshine-fork host (capability-gated extensions) | Host |
| [ClassicOldSong/moonlight-android](https://github.com/ClassicOldSong/moonlight-android) | Artemis / Moonlight Noir Android client (UX inspiration) | Client |
| [LizardByte/Sunshine](https://github.com/LizardByte/Sunshine) | Standard GameStream host | Host |
| [moonlight-stream](https://github.com/moonlight-stream) | Moonlight ecosystem / protocol | Protocol |
| [Rock88/moonlight-nx](https://github.com/rock88/moonlight-nx) | Moonlight-NX streaming foundations | Legacy |

### Special thanks

- **XITRIX** — Moonlight-Switch author and original codebase
- **ClassicOldSong** — Apollo host and Artemis (Moonlight Noir) client
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

Related discussion on Moonlight Qt: [moonlight-qt#1557](https://github.com/moonlight-stream/moonlight-qt/issues/1557) (upscaling).

This fork follows the licensing requirements of Moonlight-Switch and the upstream components it incorporates (including Apollo/Sunshine/GameStream, Borealis, FFmpeg/NVDEC, deko3d, and libnx). See [`LICENSE`](LICENSE) and relevant source-file notices for details.
