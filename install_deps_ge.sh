#!/usr/bin/env bash
set -euo pipefail
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config \
  libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev \
  libgles2-mesa-dev mesa-utils \
  can-utils usbutils iproute2 python3

echo
printf 'Dependencies installed. For direct DRM/KMS use, ensure your user is in video/render groups:\n'
printf '  sudo usermod -aG video,render "$USER"\n'
printf 'Log out/in after changing groups.\n'
