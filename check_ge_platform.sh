#!/usr/bin/env bash
set -u

echo '=== GE GEA-SBC001 / Genio 350 platform check ==='
printf 'Architecture: '; uname -m
printf 'Kernel: '; uname -r
printf 'Page size: '; getconf PAGESIZE
printf 'Board: '
tr -d '\0' </proc/device-tree/model 2>/dev/null || echo '(device tree model unavailable)'
echo

echo '--- DRM devices ---'
ls -l /dev/dri 2>/dev/null || echo 'No /dev/dri devices found'

echo '--- MediaTek / Panfrost modules ---'
lsmod | grep -E 'panfrost|mediatek|mtk_drm|drm' || true

echo '--- SDL / Mesa ---'
pkg-config --modversion sdl2 2>/dev/null || echo 'SDL2 development package not installed'
pkg-config --modversion SDL2_image 2>/dev/null || echo 'SDL2_image development package not installed'
pkg-config --modversion SDL2_ttf 2>/dev/null || echo 'SDL2_ttf development package not installed'
command -v glxinfo >/dev/null && glxinfo -B 2>/dev/null | grep -E 'OpenGL vendor|OpenGL renderer|OpenGL version' || true
command -v eglinfo >/dev/null && eglinfo -B 2>/dev/null | head -80 || true

echo '--- USB devices ---'
lsusb 2>/dev/null || true

echo '--- CAN interfaces ---'
ip -details link show type can 2>/dev/null || echo 'No SocketCAN interface currently configured'

echo
echo 'Expected target: MT8365/Genio 350 + Mali-G52/Panfrost + 1280x800 logical cluster.'
echo 'If the physical panel is exposed as 800x1280, --rotate auto rotates the completed scene by 90 degrees.'
