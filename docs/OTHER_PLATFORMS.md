# Other platforms (desktop, Android, Vita)

Artemis Switch is **Switch-first**. The same tree still builds the Moonlight-Switch
portable platforms. Switch-only behaviors are compile-time gated so desktop and
mobile builds do not inherit Horizon-specific stream teardown or clock UI.

## Feature flags (CMake)

| Option | Switch default | Other platforms | Effect |
| --- | --- | --- | --- |
| `ARTEMIS_END_STREAM_ON_FOCUS_LOSS` | **ON** | **OFF** | End the stream when the app window loses focus (Switch sleep / HOME). Off on desktop so alt-tab does not kill a stream. |
| `ARTEMIS_SWITCH_RUNTIME_CLOCKS` | **ON** | **OFF** | Read-only CPU/GPU/EMC/battery rows in the Performance tab (`clkrst` / `pcv`). |
| `ARTEMIS_CLEAR_RUMBLE_ON_STREAM_START` | **ON** | **ON** | Clear pad rumble once when a stream view starts (safe everywhere). |

Override examples:

```bash
# Desktop: keep focus-loss terminate disabled (default)
cmake -B build/desktop -DPLATFORM_DESKTOP=ON \
  -DARTEMIS_END_STREAM_ON_FOCUS_LOSS=OFF

# Force Switch-like focus terminate on a desktop debug build
cmake -B build/desktop -DPLATFORM_DESKTOP=ON \
  -DARTEMIS_END_STREAM_ON_FOCUS_LOSS=ON
```

Portable reliability fixes that stay **enabled on every platform**:

- Reject Sunshine all-zero MACs (`00:00:00:00:00:00`) for host identity and WoL
- Surface mDNS / discovery failures in Add Host UI
- Skip STUN during local discovery where applicable

## Nintendo Switch (primary)

See [README.md](../README.md#nintendo-switch-build) and [NRO_INSTALL.md](NRO_INSTALL.md).

```bash
cmake -B build/switch -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON
cmake --build build/switch --target Moonlight.nro --parallel
cp build/switch/Moonlight.nro dist/nro/Artemis-Switch.nro
```

## Linux / SteamOS

See [linux-distribution.md](linux-distribution.md) for packages (AppImage, DEB, RPM, Steam Runtime).

Typical desktop configure (toolchain / deps vary by distro):

```bash
cmake -B build/desktop -DCMAKE_BUILD_TYPE=Release -DPLATFORM_DESKTOP=ON
cmake --build build/desktop --parallel
```

## Windows

Use the repo Windows workflow as the reference (`\.github/workflows/windows.yml`).
MSYS2 / UCRT64 or CLANGARM64 triplets are used in CI.

```bash
cmake -B build/windows -DCMAKE_BUILD_TYPE=Release -DPLATFORM_DESKTOP=ON
cmake --build build/windows --parallel
```

## macOS

See CI `\.github/workflows/macos.yml`. Desktop Borealis + Metal / OpenGL paths follow upstream Moonlight-Switch.

```bash
cmake -B build/macos -DCMAKE_BUILD_TYPE=Release -DPLATFORM_DESKTOP=ON
cmake --build build/macos --parallel
```

## Android / PS Vita

Follow upstream Moonlight-Switch platform docs and CI workflows
(`android-apk.yml`, `psvita.yml`). Artemis Switch-only UI (deko3d clocks,
focus-loss terminate) remains off unless you explicitly enable the CMake flags
above on a platform that supports them.

## Portable tests (all hosts)

```bash
python tests/i18n_consistency_test.py
cmake -S tests -B build/tests -DCMAKE_BUILD_TYPE=Release
cmake --build build/tests --parallel
ctest --test-dir build/tests --output-on-failure
```

These tests do not require a Switch or GPU and should pass on Linux/Windows/macOS CI runners.
