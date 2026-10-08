#!/usr/bin/env python3
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(text: str, needle: str, source: str):
    assert needle in text, f"{source} is missing required release contract: {needle}"


def split_jobs(text: str):
    """Maps job name -> its YAML text, without needing a YAML parser in CI."""
    _, _, body = text.partition("\njobs:\n")
    jobs, current = {}, None
    for line in body.splitlines():
        match = re.match(r"^  ([A-Za-z0-9_-]+):\s*$", line)
        if match:
            current = match.group(1)
            jobs[current] = []
        elif current:
            jobs[current].append(line)
    return {name: "\n".join(lines) for name, lines in jobs.items()}


def check_test_jobs_fetch_submodules():
    # The unit suite compiles sources from extern/wg-nx and includes
    # extern/borealis. A plain actions/checkout leaves both empty, so every job
    # that builds the suite has to fetch them.
    for name in ("unit-tests.yml", "feature-integration-ci.yml"):
        text = (ROOT / ".github/workflows" / name).read_text(encoding="utf-8")
        for job, block in split_jobs(text).items():
            if "cmake -S tests" in block:
                assert "submodule" in block, (
                    f"{name} job '{job}' builds the unit suite without fetching submodules"
                )
                assert "extern/wg-nx" in block or "recursive" in block, (
                    f"{name} job '{job}' does not fetch extern/wg-nx"
                )


def check_unit_suite_keeps_asserts_live():
    # Release adds -DNDEBUG, which compiles every assert() away and turns the
    # suite into a no-op. The CMake file must strip it and keep the guard test.
    cmake = (ROOT / "tests/CMakeLists.txt").read_text(encoding="utf-8")
    assert "NDEBUG" in cmake, "tests/CMakeLists.txt no longer strips NDEBUG"
    assert "assert_enabled_test" in cmake, "assert guard test is not registered"
    assert (ROOT / "tests/assert_enabled_test.cpp").exists()


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
        "Artemis-Switch.nro",
        "Artemis-Switch.elf",
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
        "Artemis-Switch.nro",
        "Artemis-Switch.elf",
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
    ]:
        require(integration, needle, "feature-integration-ci.yml")

    for needle in [
        "rsync -a",
        "SOURCE_INFO.txt",
        "source.tar.gz",
        "source.zip",
        "test -s",
        "--exclude='.git'",
        "--exclude='build'",
    ]:
        require(package, needle, "package-release-source.sh")

    check_test_jobs_fetch_submodules()
    check_unit_suite_keeps_asserts_live()

    print(
        "Release contract OK: quality gate, Switch binary, debug ELF, "
        "source archives, prerelease handling and checksums are required"
    )


if __name__ == "__main__":
    main()
