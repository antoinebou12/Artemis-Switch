#!/usr/bin/env bash
set -euo pipefail

version="${1:-dev}"
out_dir="${2:-dist}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Keep archive names filesystem-safe while preserving normal semver tags.
safe_version="${version#v}"
safe_version="$(printf '%s' "$safe_version" | tr -c 'A-Za-z0-9._+-' '-')"
if [[ -z "$safe_version" ]]; then
  safe_version="dev"
fi

mkdir -p "$out_dir"
out_dir="$(cd "$out_dir" && pwd)"
stage_dir="$(mktemp -d)"
trap 'rm -rf "$stage_dir"' EXIT

rsync -a \
  --exclude='.git' \
  --exclude='*/.git' \
  --exclude='build' \
  --exclude='dist' \
  --exclude='.cache' \
  "$repo_root/" "$stage_dir/Artemis-Switch-$safe_version/"

commit="$(git -C "$repo_root" rev-parse HEAD 2>/dev/null || printf 'unknown')"
cat > "$stage_dir/Artemis-Switch-$safe_version/SOURCE_INFO.txt" <<EOF
Artemis Switch source bundle
Version: $version
Commit: $commit
Generated: $(date -u +%Y-%m-%dT%H:%M:%SZ)

This source bundle was produced from a recursive checkout and includes checked-out
submodule contents. Git metadata and build outputs are intentionally excluded.
EOF

(
  cd "$stage_dir"
  tar -czf "$out_dir/Artemis-Switch-$safe_version-source.tar.gz" "Artemis-Switch-$safe_version"
  zip -qr "$out_dir/Artemis-Switch-$safe_version-source.zip" "Artemis-Switch-$safe_version"
)

for asset in \
  "$out_dir/Artemis-Switch-$safe_version-source.tar.gz" \
  "$out_dir/Artemis-Switch-$safe_version-source.zip"; do
  test -s "$asset"
done

printf 'Created source packages for %s\n' "$version"
