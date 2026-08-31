#!/usr/bin/env python3
"""Lint changed Artemis-owned files without traversing vendored dependencies."""

from __future__ import annotations

import argparse
import ast
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp"}
TEXT_SUFFIXES = CPP_SUFFIXES | {
    ".cmake",
    ".json",
    ".md",
    ".py",
    ".sh",
    ".xml",
    ".yaml",
    ".yml",
}
TEXT_NAMES = {"CMakeLists.txt", ".clang-format", ".clang-tidy", ".editorconfig"}
EXCLUDED_PREFIXES = ("build/", "dist/", "extern/", ".git/")


def git(*arguments: str) -> str:
    result = subprocess.run(
        ["git", *arguments],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout


def is_in_scope(relative: str) -> bool:
    normalized = relative.replace("\\", "/")
    return not any(
        normalized == prefix.rstrip("/") or normalized.startswith(prefix)
        for prefix in EXCLUDED_PREFIXES
    )


def null_separated_paths(output: str) -> set[str]:
    return {path for path in output.split("\0") if path and is_in_scope(path)}


def revision_exists(revision: str) -> bool:
    return (
        subprocess.run(
            ["git", "cat-file", "-e", f"{revision}^{{commit}}"],
            cwd=ROOT,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        ).returncode
        == 0
    )


def selected_paths(arguments: argparse.Namespace) -> set[str]:
    if arguments.base:
        base = arguments.base
        if set(base) == {"0"} or not revision_exists(base):
            base = f"{arguments.head}^"
        return null_separated_paths(
            git("diff", "--name-only", "--diff-filter=ACMR", "-z", base, arguments.head)
        )

    paths = null_separated_paths(git("diff", "--name-only", "--diff-filter=ACMR", "-z"))
    paths.update(
        null_separated_paths(git("diff", "--cached", "--name-only", "--diff-filter=ACMR", "-z"))
    )
    paths.update(null_separated_paths(git("ls-files", "--others", "--exclude-standard", "-z")))
    return paths


def check_text_files(paths: list[Path]) -> list[str]:
    problems: list[str] = []
    for path in paths:
        relative = path.relative_to(ROOT).as_posix()
        try:
            contents = path.read_text(encoding="utf-8")
        except UnicodeDecodeError as error:
            problems.append(f"{relative}: not valid UTF-8 ({error})")
            continue

        if contents and not contents.endswith("\n"):
            problems.append(f"{relative}: missing final newline")
        for line_number, line in enumerate(contents.splitlines(), start=1):
            if line.rstrip(" \t") != line:
                problems.append(f"{relative}:{line_number}: trailing whitespace")

        if path.suffix == ".py":
            try:
                ast.parse(contents, filename=relative)
            except SyntaxError as error:
                problems.append(f"{relative}:{error.lineno}: {error.msg}")
        elif path.suffix == ".json":
            try:
                json.loads(contents)
            except json.JSONDecodeError as error:
                problems.append(f"{relative}:{error.lineno}: invalid JSON: {error.msg}")
    return problems


def run_tool(command: list[str]) -> int:
    print("+", " ".join(command))
    return subprocess.run(command, cwd=ROOT, check=False).returncode


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", help="lint files changed after this Git revision")
    parser.add_argument(
        "--head", default="HEAD", help="revision paired with --base (default: HEAD)"
    )
    arguments = parser.parse_args()

    relative_paths = selected_paths(arguments)
    paths = sorted(
        (ROOT / relative for relative in relative_paths if (ROOT / relative).is_file()),
        key=lambda path: path.as_posix(),
    )
    text_paths = [path for path in paths if path.suffix in TEXT_SUFFIXES or path.name in TEXT_NAMES]
    cpp_paths = [path for path in paths if path.suffix in CPP_SUFFIXES]
    python_paths = [path for path in paths if path.suffix == ".py"]

    problems = check_text_files(text_paths)
    if problems:
        print("Repository hygiene failed:", file=sys.stderr)
        for problem in problems:
            print(f"  {problem}", file=sys.stderr)
        return 1

    if cpp_paths:
        formatter = shutil.which("clang-format")
        if not formatter:
            print("clang-format is required to lint C/C++ changes", file=sys.stderr)
            return 2
        if run_tool([formatter, "--dry-run", "--Werror", *map(str, cpp_paths)]) != 0:
            return 1

    if python_paths:
        ruff = shutil.which("ruff")
        if not ruff:
            print("Ruff is required to lint Python changes", file=sys.stderr)
            return 2
        if run_tool([ruff, "check", *map(str, python_paths)]) != 0:
            return 1
        if run_tool([ruff, "format", "--check", *map(str, python_paths)]) != 0:
            return 1

    print(
        f"Lint passed: {len(paths)} changed first-party files "
        f"({len(cpp_paths)} C/C++, {len(python_paths)} Python)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
