#!/usr/bin/env python3
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WG_NX_PIN = "c137c32829d01bdd98e81150f2d779391e13a2ab"
NETBIRD_PIN = "55d5b04fe7d666b4a2d2a324884caf6b0e926212"
TAILSCALE_LIBSODIUM_PIN = "15e6dad043d3c556e6152a576b7fe8f1caf1980b"
TAILSCALE_NGHTTP2_PIN = "86dff0f307453f9992294d245ac8074f5fe5dbd1"


def gitlink(path: str) -> str:
    output = subprocess.check_output(
        ["git", "-C", str(ROOT), "ls-files", "--stage", path], text=True
    ).strip()
    parts = output.split()
    assert len(parts) >= 3 and parts[0] == "160000", output
    return parts[1]


def main():
    modules = (ROOT / ".gitmodules").read_text(encoding="utf-8")
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    namespace = (ROOT / "cmake/NetBirdNamespace.cmake").read_text(
        encoding="utf-8"
    )
    netbird_backend = (ROOT / "cmake/NetBirdBackend.cmake").read_text(
        encoding="utf-8"
    )
    wireguard = (ROOT / "cmake/WireGuardBackend.cmake").read_text(
        encoding="utf-8"
    )
    tailscale = (ROOT / "cmake/TailscaleWgxBackend.cmake").read_text(
        encoding="utf-8"
    )
    compatibility = json.loads(
        (ROOT / "compatibility/tailscale/manifest.json").read_text(
            encoding="utf-8"
        )
    )

    assert 'path = extern/wg-nx' in modules
    assert 'url = https://github.com/jmpangilinan/wg-nx.git' in modules
    assert gitlink("extern/wg-nx") == WG_NX_PIN
    assert gitlink("extern/netbird-switch") == NETBIRD_PIN
    assert gitlink("extern/tailscale-libsodium") == TAILSCALE_LIBSODIUM_PIN
    assert gitlink("extern/tailscale-nghttp2") == TAILSCALE_NGHTTP2_PIN
    assert (ROOT / "extern/tailscale-libsodium/src/libsodium").is_dir()
    assert (ROOT / "extern/tailscale-nghttp2/lib/nghttp2_session.c").is_file()

    for required in [
        "artemis_wireguard_deps",
        "WireGuardBackend.cmake",
        "NetBirdNamespace.cmake",
        "NETBIRD_OBJCOPY",
        "artemis_tailscale_wgx_deps",
        "TailscaleWgxBackend.cmake",
        "ENABLE_TAILSCALE",
    ]:
        assert required in cmake, f"CMake missing VPN isolation contract: {required}"
    assert "ENABLE_WIREGUARD=ON needs the real wg-nx backend" not in cmake

    for required in [
        "wireguard.o",
        "lwip_tcp.o",
        "netbird_internal_",
        "--redefine-syms=",
        "netbird_init",
    ]:
        assert required in namespace, f"namespace contract missing: {required}"
    assert "libwireguard.a" in wireguard
    assert '"${_make_exe}" all' in wireguard
    assert "netbird-switch-peer-identity.patch" in netbird_backend

    # Enabling the experimental sources must never open the provider. Only a
    # reviewed live-gate update may replace null with an accepted capability.
    assert compatibility["accepted_capability_version"] is None
    assert "HTTP/2-over-Noise control session" in compatibility["not_implemented"]
    assert "fail-closed" in compatibility["release_gate"]

    for required in [
        "wireguard.o",
        "lwip_tcp.o",
        "tailscale_internal_",
        "--redefine-syms=",
        "Unexpected Tailscale wgx archive members",
    ]:
        assert required in tailscale, f"Tailscale isolation contract missing: {required}"

    print("VPN build contract OK: three independent stacks, pins, and symbol isolation")


if __name__ == "__main__":
    main()
