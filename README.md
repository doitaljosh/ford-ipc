# Ford IPC — GE GEA-SBC001 / Genio 350, 1280×800

This is the GE refrigerator backport of the hardened Ford IPC test project. The safety/core code is carried forward unchanged from the ASIL-B-oriented branch; the platform and HMI layer are adapted to the **GE GEA-SBC001 / MediaTek Genio 350** target and its **1280×800 logical landscape layout**.

The physical GE panel may enumerate as **800×1280 portrait**. `--rotate auto` detects a portrait SDL output and rotates the completed 1280×800 scene 90 degrees, so the HMI itself always renders in landscape coordinates.

## Ford Antenna typography

The GE HMI uses the extracted `FordAntennaCondWGL-Light.otf` globally for normal text and `FordAntennaCondWGL-Reg.otf` for emphasized/bold text, including traffic-sign text. There is no system-font fallback. The active PRNDL range is bold Antenna Regular in Ford orange `#FF9600`; inactive ranges remain Antenna Light in dim gray. The proprietary font files are extracted locally with `extract_cluster_fonts.py` and are not distributed in this package.

## What is preserved from the hardened branch

- native Ford Antenna OTF/TTF runtime rendering through SDL_ttf/FreeType — no glyph atlas
- fixed-capacity safety-relevant shared state
- bounded shared-state access
- separate render-thread, CAN-thread, and accepted-CAN-data heartbeats
- stale-data invalidation and fail-silent safe-state behavior
- CAN controller error/bus-off handling
- strict decoder plausibility checks
- static safety-core linkage by default
- emergency primitive-only HMI fallback after resource/render failure
- multi-page center display
- startup gauge/telltale POST
- yellow/red warning popup manager
- animated door-ajar popup with real per-door hinge motion
- steering-wheel Up/Down/OK behavior

The source is **ASIL-B-oriented/hardened**, not independently ISO 26262 certified. See `SAFETY_ASIL_B.md` and `TRACEABILITY.md`.

## GE 1280×800 HMI layout

The former 1480×320 strip composition is not simply stretched. The GE HMI is reflowed as a true 1280×800 scene:

- large 360-source Ford speedometer at left
- large Ford tachometer at right
- tall central driver-assist / multi-page region
- compact left/right BLIS + CTA indicators flanking the center region
- top telltale row
- full-width lower vehicle-status band
- enlarged yellow/red popups centered over the HMI
- enlarged vehicle/door rendering for the door-ajar popup and Vehicle page
- bottom-right **OK** dismiss control on warnings

The Driver Assist page keeps the newer behavior: the small top-down host vehicle is **not** rendered below the ACC gap bars. Selected gap is shown with the green Ford trapezoids.

## Fonts

The raw Ford font files are not redistributed. Extract them from your IPC firmware into:

```text
assets/ford_ipc/private_fonts/
```

Use:

```bash
python3 extract_cluster_fonts.py /path/to/LV4T-14C088-AJ-or-extracted-firmware
./check_cluster_fonts.sh
```

Required files:

```text
FordAntennaCondWGL-Light.otf
FordAntennaCondWGL-Reg.otf
```

The Cond Light face is used globally for normal/inactive text and Cond Regular for emphasized/bold text, including traffic-sign text. There is no alternate runtime typeface or system-font fallback.

Because the GE target renders at 1280×800 logical resolution, text is rasterized from the original font at the actual render scale rather than magnifying a fixed atlas.

## Install dependencies on the GE Ubuntu image

```bash
./install_deps_ge.sh
```

Important packages include SDL2, SDL2_image, SDL2_ttf/FreeType, Mesa/GLES, CMake and can-utils.

Check the target first if desired:

```bash
./check_ge_platform.sh
```

## Build

```bash
./build_ge.sh
```

This performs a strict Release build with warnings-as-errors, static core linkage, and then runs the eight unit/core tests.

Executable:

```text
build/ford_adas_ipc
```

## Run

Standby / no CAN:

```bash
./run_ge_nocan.sh
```

Animated demo:

```bash
./run_ge_demo.sh
```

Live vehicle CAN:

```bash
./setup_usb_can.sh can0 500000
./run_ge_can.sh can0
```

Direct KMS/DRM from a console session:

```bash
./run_ge_kms.sh nocan
./run_ge_kms.sh demo
./run_ge_kms.sh can0
```

The application is receive-only on CAN.

## Center pages / controls

Pages:

1. Driver Assist
2. Trip Computer A
3. Trip Computer B
4. Now Playing
5. Vehicle / door status

Controls:

- Up / Down — previous/next page
- Enter / keypad Enter — steering-wheel OK equivalent; dismisses popup and resets active Trip A/B where applicable
- `F1` — Full Accessory Power Enabled
- `F2` — Washer Fluid Level Low
- `F3` — See Manual / yellow wrench
- `F4` — Powertrain Malfunction
- `F5` — Engine Over Temperature
- `F6` — Engine Oil Pressure Low
- `F7` — Charging System Malfunction
- `F8` / `F9` — driver/passenger front door toggle
- `F10` / `F11` — left/right rear door toggle
- `H` / `L` — hood/liftgate toggle
- `D` — debug overlay
- `R` — rotate output 90°
- `F` — fullscreen toggle
- Esc — exit

A door popup occurs on a **closed → open** transition. Already-open doors remain visibly open while the newly opened door swivels out. Closing a door is silent.

## Live warning mapping status

Verified live mappings remain conservative:

- verified oil-pressure request → **Engine Oil Pressure Low**
- `EngSrvcRqd_B_Rq` → yellow wrench / **See Manual**

Washer-fluid, red Powertrain Malfunction, and Engine Over Temperature UI hooks exist, but no speculative CAN bits are assigned. See `CAN_SIGNALS_MULTIPAGE.md`.

## Safety verification

Core strict Release verification:

```bash
cmake -S . -B build-core -DCMAKE_BUILD_TYPE=Release -DFORD_ADAS_BUILD_UI=OFF -DFORD_ADAS_WERROR=ON
cmake --build build-core -j"$(nproc)"
ctest --test-dir build-core --output-on-failure
```

Sanitizer/static-analysis helper:

```bash
./run_safety_checks.sh
```

`cppcheck` and `clang-tidy` are run only when installed; skipped tools are reported as skipped.

## Preview

`preview.png` / `preview_ge_layout_reference.png` show the established GE 1280×800 base composition used for the backport. The live hardened build adds the newer page/popup/native-font behavior described above.

## Ford-style warning chimes

This GE build includes a local fallback warning-chime renderer. Inspection of
the supplied IPC firmware found the Ford SBUS warning-chime commands and
`hard_chime` / `soft_chime` / `info_chime` types, but no recoverable WAV/Ogg
asset; on the production Escape the IPC normally commands the ACM/DSP to render
warning audio.

For this standalone GE board, yellow/red warning popups use a descending
two-tone pair repeated three times. Any passenger door that remains open uses a
slower repeating Ford-style bell; a newly opened door restarts that cadence.
Warnings preempt the door reminder and the door reminder resumes afterward when
needed.

```text
--chime-volume 0..100    default 72
--mute-chimes            disable local chimes
```

See `CHIME_REVERSE_ENGINEERING.md` for the firmware findings, cadence and audio
failure-containment behavior.


### Chime listening references

Only the current click-free reference clips are included under `reference/chimes/`:

- `ford_door_chime_clickfree.wav`
- `ford_warning_chime_clickfree.wav`

Both files include 200 ms of digital silence before the first tone and 150 ms after the final tail. The preroll prevents media players / DACs from waking at the same instant as the first audible sample. The application does **not** load these WAVs at runtime; it synthesizes the same 440/550/660 Hz tone family directly while keeping the SDL audio device continuously active. Older experimental and ACM-coefficient reconstruction WAVs are intentionally not shipped.
