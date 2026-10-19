# Ford IPC asset manifest

These raster files were extracted from the 2020 Ford Escape 12.3-inch digital IPC firmware packages supplied by the user for this personal reverse-engineering project. They are kept here as local runtime resources so the Raspberry Pi display can use the genuine IPC artwork rather than approximations.

No raw font files are included in the downloadable project. The renderer now loads the original Ford Antenna font files directly at runtime through SDL_ttf/FreeType instead of using pre-rasterized glyph atlases. Run `extract_cluster_fonts.py` against your own `LV4T-14C088-AJ` IPC package to populate `assets/ford_ipc/private_fonts/`.


| Project asset | Extracted source image |
| --- | --- |
| lane_left_white.png | 027d273c_176x129.png |
| lane_right_white.png | 027d3f98_177x128.png |
| lane_left_green.png | 027cf5f0_176x129.png |
| lane_right_green.png | 027d0f48_177x128.png |
| lane_left_red.png | 027d7984_176x129.png |
| lane_right_red.png | 027d8f0c_176x128.png |
| lane_left_amber.png | 027da3b4_176x129.png |
| lane_right_amber.png | 027dbd4c_177x128.png |
| host_vehicle.png | 029b8650_90x192.png |
| lead_vehicle_near.png | 02b28ffc_438x212.png |
| lead_vehicle_mid.png | 02b33ccc_438x205.png |
| lead_vehicle_far.png | 02b3c450_438x200.png |
| lead_vehicle_very_far.png | 02b42ec8_438x196.png |
| follow_gap_1.png | 02b21458_317x49.png |
| follow_gap_2.png | 02b225f8_317x73.png |
| follow_gap_3.png | 02b2437c_317x88.png |
| follow_gap_4.png | 02b267a0_317x99.png |
| speed_limit_blank.png | 0379d3bc_91x91.png |
| no_passing.png | 02a12624_91x91.png |

The three parking-proximity artwork files were retained as additional visual reference but are not required by the current renderer.

## Background and Ford Antenna runtime fonts

| Resource | Firmware source / use |
| --- | --- |
| `ipc_background.png` | Cropped/resampled from extracted IPC texture `032117f8_1920x686.png` |
| `FordAntennaCondWGL-Light.otf` | Extracted locally from the supplied IPC `ifs2.bin`; normal IPC labels |
| `FordAntennaCondWGL-Reg.otf` | Extracted locally from the supplied IPC `ifs2.bin`; emphasized/PRNDL text |

The three font files above are **runtime inputs but are not bundled**. The extraction helper writes them from the user's own firmware into `private_fonts/`.

## Telltale assets added for the full IPC view

| Project asset | Extracted source image | Live decode status |
| --- | --- | --- |
| `telltale_mil.png` | `03609700_40x27.png` | live: ECM MIL request |
| `telltale_abs.png` | `03601d20_81x63.png` | live: ABS lamp/fault |
| `telltale_traction.png` | `032f4404_82x82.png` | live: traction/stability lamp request |
| `telltale_oil.png` | `03610ed0_41x17.png` | live: low oil-pressure warning |
| `telltale_brake.png` | `036082a4_76x23.png` | live: low brake-fluid warning |
| `telltale_battery.png` | `0330e0e4_89x58.png` | artwork only; CAN signal not yet assigned |
| `telltale_seatbelt.png` | `03607df4_33x41.png` | artwork only; CAN signal not yet assigned |
| `telltale_tpms.png` | `03619d1c_38x37.png` | artwork only; CAN signal not yet assigned |
| `telltale_airbag.png` | `03603620_41x37.png` | artwork only; CAN signal not yet assigned |

## Derived articulated vehicle layers

The following PNGs are derived locally from the genuine `host_vehicle.png` asset. They split the top-down vehicle into independently animated body panels; no replacement car artwork is introduced.

| Resource | Use |
| --- | --- |
| `vehicle_base.png` | Vehicle body with panel recesses/openings |
| `vehicle_door_df.png` / `_shadow.png` | Driver-front door hinged animation |
| `vehicle_door_pf.png` / `_shadow.png` | Passenger-front door hinged animation |
| `vehicle_door_rl.png` / `_shadow.png` | Left-rear door hinged animation |
| `vehicle_door_rr.png` / `_shadow.png` | Right-rear door hinged animation |
| `vehicle_hood.png` / `_shadow.png` | Hood opening animation |
| `vehicle_liftgate.png` / `_shadow.png` | Liftgate opening animation |

They can be regenerated from `host_vehicle.png` with `tools_make_vehicle_panels.py`.

Global runtime typography uses only `FordAntennaCondWGL-Light.otf` and `FordAntennaCondWGL-Reg.otf`; no alternate sign/typeface file is required.
