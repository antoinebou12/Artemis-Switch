#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(text: str, needle: str, source: str):
    assert needle in text, f"{source} is missing required release contract: {needle}"


def main():
    release_path = ROOT / ".github/workflows/release.yml"
    switch_path = ROOT / ".github/workflows/docker-image.yml"
    integration_path = ROOT / ".github/workflows/feature-integration-ci.yml"
    package_path = ROOT / "scripts/package-release-source.sh"

    assert release_path.exists(), "Release workflow is missing"
    assert switch_path.exists(), "Switch reusable build workflow is missing"
    assert integration_path.exists(), "Feature/integration workflow is missing"
    assert package_path.exists(), "Source packaging helper is missing"

    release = release_path.read_text(encoding="utf-8")
    switch = switch_path.read_text(encoding="utf-8")
    integration = integration_path.read_text(encoding="utf-8")
    package = package_path.read_text(encoding="utf-8")

    for needle in [
        "tags:",
        "- 'v*'",
        "workflow_dispatch:",
        "contents: write",
        "./.github/workflows/feature-integration-ci.yml",
        "./.github/workflows/docker-image.yml",
        "needs: quality-gate",
        "startsWith(github.ref, 'refs/tags/v')",
        "artemi-switch.nro",
        "artemi-switch.elf",
        "source.tar.gz",
        "source.zip",
        "SHA256SUMS.txt",
        "sha256sum",
        "--verify-tag",
        "--generate-notes",
        "--prerelease",
        "gh release create",
        "gh release upload",
        "--clobber",
    ]:
        require(release, needle, "release.yml")

    for needle in [
        "actions/checkout@v4",
        "actions/upload-artifact@v4",
        "submodules: recursive",
        "artemi-switch.nro",
        "artemi-switch.elf",
        "if-no-files-found: error",
        "test -s build/switch/Moonlight.nro",
        "test -s build/switch/Moonlight.elf",
    ]:
        require(switch, needle, "docker-image.yml")

    for needle in [
        "workflow_call:",
        "tests/i18n_consistency_test.py",
        "tests/release_contract_test.py",
        "release-package-contract:",
        "ctest --test-dir build/tests",
        "ctest --test-dir build/integration",
        "-fsanitize=address,undefined",
        "artemi-switch-0.0.0-ci-source.tar.gz",
    ]:
        require(integration, needle, "feature-integration-ci.yml")

    for needle in [
        "rsync -a",
        "SOURCE_INFO.txt",
        "artemi-switch source bundle",
        "Author: antoinebou12",
        "source.tar.gz",
        "source.zip",
        "test -s",
        "--exclude='.git'",
        "--exclude='build'",
    ]:
        require(package, needle, "package-release-source.sh")

    print(
        "Release contract OK: artemi-switch branding, quality gate, Switch binary, "
        "debug ELF, source archives, prerelease handling and checksums are required"
    )


if __name__ == "__main__":
    main()
