#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build"
rm -rf "$BUILD"
cmake -S "$ROOT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DFORD_ADAS_BUILD_UI=ON \
  -DFORD_ADAS_STATIC_CORE=ON \
  -DFORD_ADAS_NATIVE_TUNE=OFF \
  -DFORD_ADAS_WERROR=ON
cmake --build "$BUILD" -j"$(nproc)"
ctest --test-dir "$BUILD" --output-on-failure
printf '\nBuilt: %s/build/ford_adas_ipc\n' "$ROOT"
printf 'Logical HMI: 1280x800\nSafety core linkage: static\n'
