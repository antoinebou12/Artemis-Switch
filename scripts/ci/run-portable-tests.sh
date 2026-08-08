#!/usr/bin/env bash
set -euo pipefail

if [[ ! -f tests/CMakeLists.txt ]]; then
  echo "::error::tests/CMakeLists.txt is missing. This branch has no portable test harness."
  exit 1
fi

rm -rf build-tests
cmake -S tests -B build-tests -DCMAKE_BUILD_TYPE=Release
cmake --build build-tests --parallel 2
ctest --test-dir build-tests --output-on-failure
