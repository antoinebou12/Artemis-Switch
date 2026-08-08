#!/usr/bin/env bash
set -euo pipefail

rm -rf build-integration
cmake -S ci/integration -B build-integration -DCMAKE_BUILD_TYPE=Release
cmake --build build-integration --parallel 2
ctest --test-dir build-integration --output-on-failure
