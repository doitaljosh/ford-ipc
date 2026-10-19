#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BIN="$ROOT/build/ford_adas_ipc"
[[ -x "$BIN" ]] || { echo "Build first with: $ROOT/build_ge.sh" >&2; exit 2; }
export SDL_RENDER_DRIVER="${SDL_RENDER_DRIVER:-opengles2}"
exec "$BIN" --rotate auto --renderer "$SDL_RENDER_DRIVER" "$@"
