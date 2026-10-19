#!/usr/bin/env bash
# Configure a native SocketCAN USB adapter (gs_usb/candleLight/etc.).
set -euo pipefail
IFACE="${1:-can0}"
BITRATE="${2:-500000}"
sudo modprobe can || true
sudo modprobe can_raw || true
sudo modprobe can_dev || true
sudo modprobe gs_usb || true
if ! ip link show "$IFACE" >/dev/null 2>&1; then
  echo "SocketCAN interface '$IFACE' not found."
  echo "Check lsusb/dmesg. For SLCAN serial adapters create the SocketCAN interface first."
  exit 1
fi
sudo ip link set "$IFACE" down 2>/dev/null || true
sudo ip link set "$IFACE" type can bitrate "$BITRATE" restart-ms 100
sudo ip link set "$IFACE" up
ip -details link show "$IFACE"
echo "Configured $IFACE at $BITRATE bit/s."
