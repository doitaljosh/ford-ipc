#!/usr/bin/env bash
# Direct DRM/KMS launch for a console session (no GNOME/Wayland compositor).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
BIN="$ROOT/build/ford_adas_ipc"
MODE="${1:-nocan}"
shift || true
[[ -x "$BIN" ]] || { echo "Build first with: $ROOT/build_ge.sh" >&2; exit 2; }
export SDL_VIDEODRIVER=kmsdrm
export SDL_RENDER_DRIVER=opengles2
case "$MODE" in
  demo) exec "$BIN" --demo --rotate auto --renderer opengles2 "$@" ;;
  nocan) exec "$BIN" --rotate auto --renderer opengles2 "$@" ;;
  *) exec "$BIN" --can "$MODE" --rotate auto --renderer opengles2 "$@" ;;
esac
