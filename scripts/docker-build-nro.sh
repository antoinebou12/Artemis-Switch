#!/usr/bin/env bash
# Container-side build driver (devkita64), mirroring .github/workflows/docker-image.yml.
# HOST-SAFE: resources/font is *tracked source* on the host, so it is moved aside
# (not deleted) and always restored via a trap, even if the build fails.
set -eu

# devkita64 sets DEVKITPRO=/opt/devkitpro, but netbird-switch/build.sh defaults
# PORTLIBS_PATH to /c/devkitPro/portlibs unless exported here.
export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export PORTLIBS_PATH="${PORTLIBS_PATH:-/opt/devkitpro/portlibs}"

cd /workspace

FONT_DIR="resources/font"
FONT_BAK="resources/.font.build-backup"

restore_fonts() {
    if [ -d "$FONT_BAK" ]; then
        rm -rf "$FONT_DIR"
        mv "$FONT_BAK" "$FONT_DIR"
        echo "== restored resources/font =="
    fi
}
trap restore_fonts EXIT

echo "== move bundled fonts aside (restore on exit) =="
if [ -d "$FONT_DIR" ] && [ ! -d "$FONT_BAK" ]; then
    mv "$FONT_DIR" "$FONT_BAK"
fi

echo "== removing conflicting enet package (container-only) =="
if dkp-pacman -Q switch-enet >/dev/null 2>&1; then
    dkp-pacman --noconfirm -R switch-enet || true
fi

echo "== configure (compatibility pass) =="
cmake -B build/switch -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON \
    || echo "compat-pass returned nonzero (allowed)"

echo "== configure =="
cmake -B build/switch -DCMAKE_BUILD_TYPE=Release -DPLATFORM_SWITCH=ON -DUSE_DEKO3D=ON

echo "== build NRO =="
cmake --build build/switch --target Moonlight.nro --parallel "$(nproc)"

echo "== prepare artifacts =="
test -s build/switch/Moonlight.nro
test -s build/switch/Moonlight.elf
mkdir -p dist/nro
cp build/switch/Moonlight.nro dist/nro/Artemis-Switch.nro
cp build/switch/Moonlight.elf dist/nro/Artemis-Switch.elf
sha256sum dist/nro/Artemis-Switch.nro > dist/nro/Artemis-Switch.nro.sha256

echo "== DONE =="
ls -la dist/nro/Artemis-Switch.nro