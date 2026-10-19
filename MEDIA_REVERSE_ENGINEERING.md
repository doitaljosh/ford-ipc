# Now Playing transport — current reverse-engineering status

## Confirmed on normal CAN

`0x3F1 APIM_Send_Signals1_FD1` carries numeric media state used by this project:

- active track number
- number of tracks

The APIM VMCU firmware itself contains `0x3F1` in its CAN-ID table.

## Confirmed inside the factory IPC HMI

The 2020 Escape IPC QNX image contains explicit handling for
`SBUS_CONTROL_MIRROR_3_DATA`.  That handler converts Ford SBUS strings and sends HMI
traps for:

- `SyncMediaTrackName` -> `INFOTAINMENT_SYNC_MEDIA_TRACK_TITLE`
- `SyncMediaArtistName` -> `INFOTAINMENT_SYNC_MEDIA_ARTIST_NAME`
- media icon/source type
- SYNC media track number

The HMI also exposes source-specific fields for Bluetooth and USB track/artist data.
This establishes that variable-length text reaches the cluster over the IPC's
VMF/SBUS/virtual-gateway path rather than being stored directly in the normal
`0x3F1` eight-byte CAN payload.

## Album and connected device

The current pass has not isolated a proven classic-CAN or SBUS field for album name
or the connected-device display name.  The IPC firmware contains source/device HMI
logic, but that is not enough to assign a wire-format field safely.

## Implication for this Raspberry Pi project

With one ordinary USB SocketCAN interface, the honest implementation is:

- Decode track index/count directly from `0x3F1`.
- Keep title/artist/album/device supplied through `--media-file` until the VMF/SBUS
  framing or another proven transport is reconstructed.

A live capture while changing tracks/sources is the next useful data set.  The
included `capture_apim_probe.sh` records the APIM-adjacent `0x3F0-0x3FF` CAN range for
correlation, but plain-text metadata is not expected there based on the firmware.
