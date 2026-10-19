# Confirmed CAN signals used by the multi-page IPC

These fields come from the Ford/Lincoln CGEA database used while developing this build.
The steering-control mappings have additionally been cross-checked against the
user-supplied SCCM application, and the APIM track-message family has been
cross-checked against the user-supplied APIM VMCU application.
The application consumes the gatewayed vehicle bus only; it does not need the private
C-CM ↔ IPMA radar CAN.

| CAN ID | Message | Signal(s) used |
|---:|---|---|
| `0x081` | `Steering_Wheel_Data2_FD1` | Down bit 0, Up bit 3, OK bit 4 |
| `0x083` | `Steering_Data_FD1` | Gap decrement bit 12, gap increment bit 11, gap toggle bit 32 |
| `0x18A` | `ACCDATA_3` | `AccTGap_D_Dsply` bits `34|3`, `AccTGap_B_Dsply` bit 35, follow mode bit 17 |
| `0x3B3` | `BodyInfo_3_FD1` | Liftgate bit 0, RL bit 48, RR bit 49, hood bit 59, passenger-front bit 60, driver-front bit 61 |
| `0x3F1` | `APIM_Send_Signals1_FD1` | active track `7|32`, number of tracks `39|16` |

## ACC gap choice

`0x083` contains the momentary steering/SCCM button events.  The value that is most
appropriate for an IPC to render is `AccTGap_D_Dsply` on `0x18A`, because that is the
processed display-state output from the IPMA.  The code uses `0x18A` as authoritative
and the `0x083` switches as an immediate/fallback update.

The generic database defines gap states 1 through 5.  This UI deliberately renders
only the requested four-bar Escape presentation: values 1–4 map directly and a value
of 5 is visually clamped to four bars.

## Now-playing strings

The APIM VMCU firmware confirms the `0x3F0-0x3FD` CAN family, including `0x3F1`,
but does not expose a classic-CAN payload carrying the variable-length title/artist
strings.  Cross-checking the factory IPC QNX image shows those strings arriving in
`SBUS_CONTROL_MIRROR_3_DATA` through the IPC VMF/SBUS path.  Therefore title, artist,
album and device name remain on the optional `--media-file` bridge instead of being
assigned invented CAN IDs.  Track number/count remain native CAN data.

## Popup / warning inputs added in animated-vehicle revision

| CAN ID | Frame / signal | Use |
|---|---|---|
| `0x3B3` | `BodyInfo_3_FD1` door/hood/tailgate bits | Integrated panel animation; red popup only on passenger-door closed -> open transitions |
| `0x3B3` | `Ignition_Status` (`7|4`, value `2` = Accessory) | Full Accessory Power Enabled transient popup |
| `0x179` | `EngineData_7 / EngSrvcRqd_B_Rq` (`19|1`) | yellow See Manual / wrench popup |
| `0x42C` | `Low_Voltage_Power_Data_FD1 / PwSysULoFalt_D_Stat` (`12|4`) | red Charging System Malfunction for output/voltage/monitor fault states |

The exact CX482 signals for **Washer Fluid Level Low**, **Powertrain Malfunction**, and **Engine Over Temperature** are not asserted here because they were not verified in the available generic DBC/module evidence. Their popup renderers are implemented and can be exercised from the GE 1280x800 tester without inventing live CAN mappings. **Engine Oil Pressure Low** is driven by the already-verified oil-pressure request in `0x421`.
