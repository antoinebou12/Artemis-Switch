#!/usr/bin/env python3
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WG_NX_PIN = "c137c32829d01bdd98e81150f2d779391e13a2ab"
NETBIRD_PIN = "55d5b04fe7d666b4a2d2a324884caf6b0e926212"


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
    wireguard = (ROOT / "cmake/WireGuardBackend.cmake").read_text(
        encoding="utf-8"
    )

    assert 'path = extern/wg-nx' in modules
    assert 'url = https://github.com/jmpangilinan/wg-nx.git' in modules
    assert gitlink("extern/wg-nx") == WG_NX_PIN
    assert gitlink("extern/netbird-switch") == NETBIRD_PIN

    for required in [
        "artemis_wireguard_deps",
        "WireGuardBackend.cmake",
        "NetBirdNamespace.cmake",
        "NETBIRD_OBJCOPY",
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

    print("VPN build contract OK: independent pins, builds, and symbol isolation")


if __name__ == "__main__":
    main()
