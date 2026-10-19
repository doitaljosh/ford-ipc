#!/usr/bin/env bash
set -euo pipefail
IFACE="${1:-can0}"
OUT="${2:-apim_3f0_3ff_$(date +%Y%m%d_%H%M%S).log}"
echo "Capturing APIM-adjacent CAN IDs 0x3F0-0x3FF on ${IFACE} -> ${OUT}"
echo "Change song, source, album, Bluetooth/USB device, and track while capturing."
echo "Ctrl-C to stop."
candump -L "${IFACE},3F0:7F0" | tee "$OUT"
