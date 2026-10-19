#!/usr/bin/env bash
set -euo pipefail
IFACE="${1:-can0}"
echo "Capturing SCCM/menu/gap frames on ${IFACE}: 0x081, 0x083, 0x18A"
echo "Press steering-wheel Up/Down/OK and change ACC gap while this runs."
exec candump -tz "${IFACE},081:7FF,083:7FF,18A:7FF"
