#!/usr/bin/env bash
# Run the repository's GitHub Actions CI locally with nektos/act.
#
# What it tests: the current HEAD commit plus every uncommitted change in the
# working tree (modified, deleted and new files), staged into a throw-away
# clone. Your own build directories are never touched, and the OneDrive/vcpkg
# trees are never copied. See docs/LOCAL_CI.md.
#
# Usage:
#   scripts/ci-local.sh                      run the four test jobs below
#   scripts/ci-local.sh --list               show what would run
#   scripts/ci-local.sh -j unit-tests.yml:portable-tests    run only that job
#   scripts/ci-local.sh --keep               keep the staged tree for inspection
#
# The Switch NRO build (docker-image.yml) is deliberately not in the default
# set: it takes an hour or more. Run it explicitly with
#   scripts/ci-local.sh -j docker-image.yml:build
set -euo pipefail

IMAGE="${ARTEMIS_ACT_IMAGE:-artemis-act:ubuntu-latest}"
ACT_BIN="${ACT_BIN:-act}"

# Submodules the portable test jobs need. Mirrors the "Fetch submodules" step
# in .github/workflows (which is skipped under act because act has no network
# checkout of its own).
TEST_SUBMODULES=(
    extern/wg-nx
    extern/borealis
    extern/tailscale-libsodium
    extern/tailscale-nghttp2
)

DEFAULT_JOBS=(
    "unit-tests.yml:portable-tests"
    "unit-tests.yml:portable-sanitizers"
    "feature-integration-ci.yml:portable-integration"
    "feature-integration-ci.yml:sanitizer-integration"
)

jobs=()
keep=0
list=0
while [ $# -gt 0 ]; do
    case "$1" in
        -j|--job) jobs+=("${2:?missing workflow:job after $1}"); shift 2 ;;
        --keep) keep=1; shift ;;
        --list) list=1; shift ;;
        -h|--help) sed -n '2,/^set -euo/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown argument: $1 (try --help)" >&2; exit 2 ;;
    esac
done
[ ${#jobs[@]} -gt 0 ] || jobs=("${DEFAULT_JOBS[@]}")

if [ "$list" -eq 1 ]; then
    printf '%s\n' "${jobs[@]}"
    exit 0
fi

for tool in "$ACT_BIN" docker git; do
    command -v "$tool" >/dev/null 2>&1 || { echo "missing required tool: $tool" >&2; exit 1; }
done
docker info >/dev/null 2>&1 || { echo "the Docker daemon is not running" >&2; exit 1; }

ROOT="$(git rev-parse --show-toplevel)"
cd "$ROOT"

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "== building act runner image $IMAGE (cmake, ninja, rsync on top of act-latest) =="
    docker build -t "$IMAGE" -f ci/act/Dockerfile ci/act
fi

STAGE="$(mktemp -d "${TMPDIR:-/tmp}/artemis-ci.XXXXXX")"
cleanup() {
    if [ "$keep" -eq 1 ]; then
        echo "staged tree kept at: $STAGE"
    else
        rm -rf "$STAGE"
    fi
}
trap cleanup EXIT

echo "== staging HEAD $(git rev-parse --short HEAD) + uncommitted changes in $STAGE =="
git clone -q --no-hardlinks "$ROOT" "$STAGE"

changed=0
while IFS= read -r -d '' path; do
    case "$path" in extern/*) continue ;; esac   # submodules come from the pinned commits
    if [ -e "$path" ]; then
        mkdir -p "$STAGE/$(dirname "$path")"
        cp -p "$path" "$STAGE/$path"
    else
        rm -f "$STAGE/$path"
    fi
    changed=$((changed + 1))
done < <(git ls-files -m -o --exclude-standard -z)
echo "   overlaid $changed uncommitted file(s)"

echo "== fetching test submodules (shallow) =="
git -C "$STAGE" submodule update --init --depth 1 "${TEST_SUBMODULES[@]}" >/dev/null

# act is a native binary; give it a path it understands under Git Bash/MSYS.
if command -v cygpath >/dev/null 2>&1; then
    STAGE_ARG="$(cygpath -m "$STAGE")"
else
    STAGE_ARG="$STAGE"
fi

failed=()
for entry in "${jobs[@]}"; do
    workflow="${entry%%:*}"
    job="${entry#*:}"
    echo
    echo "================ act: $workflow / $job ================"
    if "$ACT_BIN" workflow_dispatch \
        -C "$STAGE_ARG" \
        -W ".github/workflows/$workflow" \
        -j "$job" \
        -P "ubuntu-latest=$IMAGE" \
        --pull=false \
        --container-architecture linux/amd64; then
        echo ">> PASS  $workflow / $job"
    else
        echo ">> FAIL  $workflow / $job"
        failed+=("$workflow / $job")
    fi
done

echo
if [ ${#failed[@]} -gt 0 ]; then
    echo "FAILED jobs:"
    printf '  - %s\n' "${failed[@]}"
    exit 1
fi
echo "All ${#jobs[@]} job(s) passed."
