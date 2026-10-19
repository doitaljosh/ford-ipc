# GE target notes

Target: GE GEA-SBC001 refrigerator HMI board, MediaTek MT8365 / Genio 350 class SoC, Linux/Ubuntu, Mesa/Panfrost graphics path.

Logical cluster surface: **1280×800 landscape**.

Physical-panel handling:

- SDL output 1280×800: render with rotation 0°
- SDL output 800×1280: `--rotate auto` selects 90°

The HMI uses one fixed logical coordinate system. Page fades occur in the 1280×800 render target and rotation happens only when the completed frame is presented.

## Ford Antenna typography

The GE HMI uses the extracted `FordAntennaCondWGL-Light.otf` globally for normal text and `FordAntennaCondWGL-Reg.otf` for emphasized/bold text, including traffic-sign text. There is no system-font fallback. The active PRNDL range is bold Antenna Regular in Ford orange `#FF9600`; inactive ranges remain Antenna Light in dim gray. The proprietary font files are extracted locally with `extract_cluster_fonts.py` and are not distributed in this package.

## Visual geometry restoration

This revision restores the pre-backport gauge presentation and tightens the Driver Assist road surface:

- speedometer and tachometer faces render at their native **360x360** supersampled size so the bezel, glass, radial shading and outer shadow remain smooth;
- the separate blurred needle-shadow, specular needle and layered hub assets are retained;
- the center road/background begins at the visible lane horizon near `y=288`, rather than filling the entire center pane;
- its upper corners are approximately `x=612/668`, its lower corners terminate exactly on the center dividers at `x=390/890`, `y=660`;
- the road edges follow the same visual perspective as the Ford lane-line assets and no longer extend underneath the left/right gauge regions.


For direct KMS/DRM, use `run_ge_kms.sh` after stopping the desktop compositor and ensure the account can access `/dev/dri/card*` and `/dev/dri/renderD*`.

## Audio / chimes

SDL opens the board's default audio output for a local Ford-style warning-chime
fallback. The current renderer is matched to the user's in-car recordings:

- warning: **440 -> 550 Hz**, ~149 ms between note starts, three pairs per event;
- door: **440 -> 660 -> 550 Hz**, ~153 ms between note starts, repeated about every 1.608 s while a door remains open.

Each sine note uses a raised-cosine attack and terminal release with an overlapping
exponential tail. The SDL audio device is opened once and then kept running on
digital zero; the first 200 ms after opening are reserved as a silent hardware/mixer
warm-up before any chime may begin. This removes both note-boundary clicks and
start-of-stream pops. Use `--chime-volume 0..100` or `--mute-chimes` as needed.
If audio is unavailable, visual warnings continue and `AUDIO_OUTPUT` is latched as
a degraded fault.

The two listen-only WAVs in `reference/chimes/` also contain 200 ms of silent preroll
and are the only audio reference files shipped in the project.

## ACM-derived warning chimes

The supplied 2020 Escape Premium ACM application (`JX7T-14C302-BM`) contains
parameterized chime/envelope profiles rather than conventional WAV assets. The
actual audible note frequencies and cadence are now taken from the user's clean
vehicle recordings, which established the 440/550/660 Hz tone family. The older
763/918 Hz interpretation of two ACM coefficient pairs is retained in the reverse-
engineering notes as a historical hypothesis, but it is no longer used as audible
pitch. See `ACM_CHIME_REVERSE_ENGINEERING.md` and
`CHIME_RECORDING_MATCH_2026-10-03.md`.
