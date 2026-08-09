#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(text: str, needle: str, source: str):
    assert needle in text, f"{source} is missing required release contract: {needle}"


def main():
    release_path = ROOT / ".github/workflows/release.yml"
    switch_path = ROOT / ".github/workflows/docker-image.yml"
    package_path = ROOT / "scripts/package-release-source.sh"

    assert release_path.exists(), "Release workflow is missing"
    assert switch_path.exists(), "Switch reusable build workflow is missing"
    assert package_path.exists(), "Source packaging helper is missing"

    release = release_path.read_text(encoding="utf-8")
    switch = switch_path.read_text(encoding="utf-8")
    package = package_path.read_text(encoding="utf-8")

    for needle in [
        "tags:",
        "- 'v*'",
        "workflow_dispatch:",
        "contents: write",
        "./.github/workflows/feature-integration-ci.yml",
        "./.github/workflows/docker-image.yml",
        "Artemis-Switch.nro",
        "Artemis-Switch.elf",
        "source.tar.gz",
        "source.zip",
        "SHA256SUMS.txt",
        "gh release create",
        "gh release upload",
    ]:
        require(release, needle, "release.yml")

    for needle in [
        "actions/checkout@v4",
        "actions/upload-artifact@v4",
        "Artemis-Switch.nro",
        "Artemis-Switch.elf",
        "if-no-files-found: error",
    ]:
        require(switch, needle, "docker-image.yml")

    for needle in [
        "rsync -a",
        "SOURCE_INFO.txt",
        "source.tar.gz",
        "source.zip",
        "test -s",
    ]:
        require(package, needle, "package-release-source.sh")

    print("Release contract OK: Switch binary, debug ELF, source archives and checksums are required")


if __name__ == "__main__":
    main()
