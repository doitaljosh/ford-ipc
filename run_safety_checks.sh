#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="$ROOT/build-sanitize"

rm -rf "$BUILD"
cmake -S "$ROOT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DFORD_ADAS_BUILD_UI=OFF \
  -DFORD_ADAS_NATIVE_TUNE=OFF \
  -DFORD_ADAS_ENABLE_SANITIZERS=ON \
  -DFORD_ADAS_WERROR=ON
cmake --build "$BUILD" -j"$(nproc)"
ctest --test-dir "$BUILD" --output-on-failure

if command -v cppcheck >/dev/null 2>&1; then
  cppcheck --std=c++17 --enable=warning,style,performance,portability \
    --error-exitcode=1 --inline-suppr -I"$ROOT/include" \
    "$ROOT/src/lib/ford_decoder.cpp" "$ROOT/src/lib/can_transport.cpp" \
    "$ROOT/src/lib/safety.cpp" "$ROOT/src/lib/cluster_logic.cpp" \
    "$ROOT/src/lib/app_services.cpp"
else
  echo "cppcheck not installed; skipping static cppcheck pass" >&2
fi

if command -v clang-tidy >/dev/null 2>&1; then
  clang-tidy -p "$BUILD" \
    "$ROOT/src/lib/ford_decoder.cpp" \
    "$ROOT/src/lib/can_transport.cpp" \
    "$ROOT/src/lib/safety.cpp" \
    "$ROOT/src/lib/cluster_logic.cpp" \
    "$ROOT/src/lib/app_services.cpp"
else
  echo "clang-tidy not installed; skipping clang-tidy pass" >&2
fi

echo "Safety-oriented test pass complete. This is verification support, not ASIL certification."
