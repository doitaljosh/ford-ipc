#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
FONTDIR="${FORD_IPC_FONT_DIR:-$ROOT/assets/ford_ipc/private_fonts}"
missing=0
for f in FordAntennaCondWGL-Light.otf FordAntennaCondWGL-Reg.otf; do
  if [[ ! -f "$FONTDIR/$f" ]]; then
    echo "Missing cluster font: $FONTDIR/$f" >&2
    missing=1
  fi
done
if (( missing )); then
  cat >&2 <<EOF

Extract the fonts locally from your IPC firmware first, for example:
  python3 "$ROOT/extract_cluster_fonts.py" /path/to/LV4T-14C088-AJ.ZIP

Or set FORD_IPC_FONT_DIR to a directory containing the extracted files.
EOF
  exit 3
fi
echo "Ford Antenna cluster fonts found in: $FONTDIR"
