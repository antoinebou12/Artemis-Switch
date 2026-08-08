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
    mapfile -t conflicts < <(git diff --name-only --diff-filter=U)

    only_shared_test_manifest=true
    if [[ ${#conflicts[@]} -eq 0 ]]; then
      only_shared_test_manifest=false
    fi

    for conflict in "${conflicts[@]}"; do
      if [[ "$conflict" != "tests/CMakeLists.txt" ]]; then
        only_shared_test_manifest=false
        break
      fi
    done

    if [[ "$only_shared_test_manifest" == true ]]; then
      echo "::notice::Resolving shared tests/CMakeLists.txt in favor of the CI integration harness"
      git checkout --ours tests/CMakeLists.txt
      git add tests/CMakeLists.txt
      git commit --no-edit
    else
      echo "::error::Integration merge conflict while adding ${branch}"
      git status --short
      printf 'Unresolved files:\n'
      printf '  %s\n' "${conflicts[@]}"
      git merge --abort || true
      exit 1
    fi
  fi
  echo "::endgroup::"
done

echo "Combined feature tree created successfully."
