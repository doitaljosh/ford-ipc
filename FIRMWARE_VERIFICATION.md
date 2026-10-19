# Firmware verification notes

This build was cross-checked against the user-supplied 2020 Ford Escape APIM VMCU
and SCCM VBF files.  The original Ford firmware files are **not** redistributed in
this project.

## SCCM

Supplied files:

- `LB5T-14C579-AB.vbf` — `Flash application for Ford CD6 SCCM`, diagnostic ECU
  address `0x724`, CAN_HS.
- `LB5T-14C581-AA.vbf` — `Flash driver for Renesas RH850 F1L Single CAN`, call
  address `0xFEE00000`.

The application VBF contains one uncompressed block at `0x00002000`, length
`0x0003DD00`.  Its VBF block CRC16 is `0xF122` and independently recalculates to
`0xF122`.

The SCCM application contains a CAN/configuration table at application image
address approximately `0x3690`.  Interpreted as little-endian 32-bit values, the
sequence includes:

```
0x630  0x62F  0x5B3  0x3E6  0x35F  0x33B  0x2A1  0x083  0x081
```

The presence of `0x081` and `0x083` in the SCCM application is independent firmware
corroboration for the steering-wheel/menu and cruise/gap messages used by this
project.  The bit-level signal interpretation still comes from the Ford CGEA DBC:

- `0x081 Steering_Wheel_Data2_FD1`: Down bit 0, Up bit 3, OK bit 4.
- `0x083 Steering_Data_FD1`: ACC gap-decrease bit 12, gap-increase bit 11,
  gap-toggle bit 32.

The processed OEM display value remains `AccTGap_D_Dsply` in `0x18A ACCDATA_3`;
that is what the IPC should render when available.

## APIM VMCU

Supplied files include:

- `1U5T-14G374-EK.VBF` — `MGM VMCU application and all calibration blocks`,
  diagnostic ECU address `0x7D0`, CAN_HS.
- `1U5T-14G375-DA.VBF` — APIM calibration data.
- `5U5T-14G379-BC.VBF` and `GB5T-14G379-AA.VBF` — SYNC calibration files.
- `GB5T-14G376-AA.VBF` — APIM flash driver/SBL.

The VMCU application contains seven uncompressed blocks.  Every stored VBF block
CRC16 was independently recalculated and matched.  The executable includes section
names such as `.ivortable` and `.vletext`, consistent with a Power Architecture/e200
VLE microcontroller firmware, along with Vector/VMCU strings.

A big-endian 16-bit CAN-ID table in the extracted VMCU image contains the contiguous
range `0x3F0` through `0x3FD`, including `0x3F1`.  This supports the APIM association
of the `0x3F1` message used by the project for active track number and total track
count.

## What the firmware does *not* prove

The APIM VMCU image does not contain an identifiable classic-CAN payload definition
for variable-length song title, artist, album, or connected-device name.  Those
strings should therefore not be assigned guessed CAN IDs or bit positions.

The separate 2020 Escape IPC firmware provides a stronger clue about the actual
factory path: its QNX HMI explicitly receives `SyncMediaTrackName` and
`SyncMediaArtistName` in `SBUS_CONTROL_MIRROR_3_DATA`, then publishes them internally
as `INFOTAINMENT_SYNC_MEDIA_TRACK_TITLE` and
`INFOTAINMENT_SYNC_MEDIA_ARTIST_NAME`.  The IPC image also contains VMF/VGW client
components.  This is a richer mirrored infotainment transport, separate from the
simple `0x3F1` track-number/count frame.

For that reason the current single-CAN build keeps the optional `--media-file`
metadata bridge for title/artist/album/device name.  Track number/count remain native
CAN data.

## Firmware SHA-256

These hashes identify the exact files analyzed without redistributing them:

```
1U5T-14G374-EK.VBF  728fa5b399446e49532c18ccc089a9c8b60f1f50608d6e7fc0b6853a4d3ee1c4
1U5T-14G375-DA.VBF  f055f123034bba1d1ab83b68a7431f1716458b7fb122fa7d0a183edbdde93b29
5U5T-14G379-BC.VBF  6ad86bfdf7e8b63074898d1c3aa1a9897ab56f39bb740a0f98af4f79aa0ccd2a
GB5T-14G376-AA.VBF  fa22720393cec3f6abce6cf6cb8074505bdff61c5571b03ff79c894101abe5dc
GB5T-14G379-AA.VBF  904870c3255c629e441c17632856cf9ab9185814bdd07a50d08cb26ffa20e859
LB5T-14C579-AB.vbf  39c3b7236ec6f030b2e440d483f3c681d61aca6981939172fcb8fd8a9608fcd4
LB5T-14C581-AA.vbf  a2b56dd7eac8001953b03d98eada69f5552b53eade98757298fa1030f9c61511
```
