#!/usr/bin/env bash
set -euo pipefail
OUT="${FORD_ADAS_MEDIA_FILE:-./now_playing.txt}"
TITLE="${1:---}"
ARTIST="${2:---}"
ALBUM="${3:---}"
DEVICE="${4:---}"
TMP="${OUT}.tmp.$$"
cat > "$TMP" <<META
title=$TITLE
artist=$ARTIST
album=$ALBUM
device=$DEVICE
META
mv "$TMP" "$OUT"
echo "Updated $OUT"
