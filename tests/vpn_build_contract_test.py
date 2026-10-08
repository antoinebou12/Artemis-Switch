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


# Every tracked recipe that configures the Switch NRO. The VPN backends default
# to ON for Switch, but the release is a contract: if a default ever flips, or a
# recipe drifts, the NRO must not silently lose NetBird, Tailscale or WireGuard.
# (scripts/build-switch-nro.sh and scripts/build-nro-msys.sh are deliberately
# absent: .gitignore's `build*` keeps them local-only.)
SWITCH_BUILD_FILES = [
    ".github/workflows/docker-image.yml",
    "scripts/docker-build-nro.sh",
    "docker-compose.yml",
]
VPN_FEATURE_FLAGS = (
    "-DENABLE_WIREGUARD=ON",
    "-DENABLE_NETBIRD=ON",
    "-DENABLE_TAILSCALE=ON",
)


def logical_lines(text: str):
    """Joins backslash continuations so a wrapped cmake call is one command."""
    return text.replace("\\\r\n", " ").replace("\\\n", " ").splitlines()


def check_switch_builds_state_vpn_flags():
    for relative in SWITCH_BUILD_FILES:
        text = (ROOT / relative).read_text(encoding="utf-8")
        commands = [line for line in logical_lines(text) if "-DPLATFORM_SWITCH=ON" in line]
        assert commands, f"{relative} no longer configures a Switch build"
        for command in commands:
            for flag in VPN_FEATURE_FLAGS:
                assert flag in command, f"{relative} must pass {flag} explicitly: {command.strip()}"


def check_random_device_is_switch_guarded():
    # devkitA64's std::random_device replays one fixed sequence, which made the
    # machine, node and disco keys identical. Any code that still reaches for
    # it must also have a Switch branch that uses the kernel CSPRNG.
    for root in ("app/src/remote_access", "app/src/vpn"):
        base = ROOT / root
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if path.suffix not in {".cpp", ".hpp", ".h"}:
                continue
            code = "\n".join(
                line.split("//", 1)[0]
                for line in path.read_text(encoding="utf-8", errors="replace").splitlines()
            )
            if "random_device" in code:
                relative = path.relative_to(ROOT)
                assert "__SWITCH__" in code and "randomGet" in code, (
                    f"{relative} uses std::random_device without a Switch randomGet branch"
                )


def main():
    modules = (ROOT / ".gitmodules").read_text(encoding="utf-8")
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    namespace = (ROOT / "cmake/NetBirdNamespace.cmake").read_text(encoding="utf-8")
    netbird_backend = (ROOT / "cmake/NetBirdBackend.cmake").read_text(encoding="utf-8")
    wireguard = (ROOT / "cmake/WireGuardBackend.cmake").read_text(encoding="utf-8")
    tailscale = (ROOT / "cmake/TailscaleWgxBackend.cmake").read_text(encoding="utf-8")
    compatibility = json.loads(
        (ROOT / "compatibility/tailscale/manifest.json").read_text(encoding="utf-8")
    )

    assert "path = extern/wg-nx" in modules
    assert "url = https://github.com/jmpangilinan/wg-nx.git" in modules
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

    # The accepted capability is a reviewed live-gate value: it must be set,
    # never exceed the audited candidate, and the gate must say what is live.
    accepted = compatibility["accepted_capability_version"]
    candidate = compatibility["candidate_capability_version"]
    assert isinstance(accepted, int) and 0 < accepted <= candidate, accepted
    assert "HTTP/2-over-Noise control session" in compatibility["implemented_primitives"]
    assert "HTTP/2-over-Noise control session" not in compatibility["not_implemented"]
    assert f"Capability {accepted} accepted" in compatibility["release_gate"]
    # Direct UDP paths stay opt-in until proven on more networks.
    assert "off by default" in compatibility["release_gate"]
    overlap = set(compatibility["implemented_primitives"]) & set(
        compatibility["not_implemented"]
    )
    assert not overlap, f"listed as both implemented and not: {overlap}"

    for required in [
        "wireguard.o",
        "lwip_tcp.o",
        "tailscale_internal_",
        "--redefine-syms=",
        "Unexpected Tailscale wgx archive members",
    ]:
        assert required in tailscale, f"Tailscale isolation contract missing: {required}"

    check_switch_builds_state_vpn_flags()
    check_random_device_is_switch_guarded()

    print("VPN build contract OK: three independent stacks, pins, and symbol isolation")


if __name__ == "__main__":
    main()
