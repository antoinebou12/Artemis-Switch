#!/usr/bin/env bash
set -euo pipefail

manifest="${1:-ci/feature-branches.txt}"

if [[ ! -f "$manifest" ]]; then
  echo "::error::Feature branch manifest not found: $manifest"
  exit 1
fi

git config user.name "Artemis-Switch CI"
git config user.email "artemis-switch-ci@users.noreply.github.com"

mapfile -t branches < <(grep -Ev '^\s*(#|$)' "$manifest")

for branch in "${branches[@]}"; do
  echo "::group::Fetch $branch"
  git fetch --no-tags origin "+refs/heads/${branch}:refs/remotes/origin/${branch}"
  echo "::endgroup::"
done

for branch in "${branches[@]}"; do
  echo "::group::Merge $branch"
  if ! git merge --no-edit --no-ff "origin/${branch}"; then
    echo "::error::Integration merge conflict while adding ${branch}"
    git status --short
    git diff --name-only --diff-filter=U || true
    exit 1
  fi
  echo "::endgroup::"
done

echo "Combined feature tree created successfully."
