# Verification results — 2026-10-03

These results are development evidence for this GE 1280×800 source snapshot. They are not an ISO 26262 certification result and do not replace target-level verification, qualified-tool arguments, structural coverage, or lifecycle work products.

## Core strict Release build

Configuration:

```text
CMAKE_BUILD_TYPE=Release
FORD_ADAS_BUILD_UI=OFF
FORD_ADAS_STATIC_CORE=ON
FORD_ADAS_WERROR=ON
```

Result: **8/8 CTest executables passed** after the GE backport and audible-warning extension.

The tests use the always-active `FORD_REQUIRE` mechanism instead of the C `assert()` macro, so the checks remain enabled when `NDEBUG` is defined.

## ASan + UBSan core build

`./run_safety_checks.sh` completed successfully after the GE backport and audible-warning extension.

Result: **8/8 CTest executables passed** with AddressSanitizer and UndefinedBehaviorSanitizer enabled; no sanitizer findings were reported by the test run.

`cppcheck` and `clang-tidy` were not installed in the verification environment and were explicitly reported as skipped.

## GE HMI / chime compile-front-end checks

The GE-backported HMI/configuration sources passed the project warning set with `-Werror` under `g++ -fsyntax-only`. The new `src/lib/chime_audio.cpp` also passed the same strict warning set using the declaration-only SDL shim.

The audible-warning unit/integration checks additionally verify that popup activation sequence IDs advance deterministically and that `AUDIO_OUTPUT` is classified as a degraded fault rather than forcing the cluster safe-state.

The verification container did not contain the actual SDL2/SDL2_image/SDL2_ttf development headers, so a temporary declaration-only SDL shim was used solely to exercise C++ syntax/type integration. This is **not** evidence of a successful SDL link, Panfrost/Mesa integration, or execution on the GE board.

## Shell scripts

The GE build/run/platform scripts are checked with `bash -n` before release packaging.

## Still required on the GE GEA-SBC001 target

At minimum, retain controlled evidence for:

- full native SDL2/SDL2_image/SDL2_ttf compile and link on the Genio 350 image
- 1280×800 landscape and 800×1280 portrait/auto-rotation display verification
- Panfrost/Mesa/OpenGL ES hardware acceleration, with software rendering rejected
- live SocketCAN receive and bus-off/restart fault injection
- no-CAN/data-timeout reaction
- missing/corrupt Ford font and PNG resource fault injection
- GPU/display failure behavior and emergency primitive-only HMI fallback
- scheduler starvation and watchdog timing
- long-duration operation and memory pressure
- power interruption/startup/shutdown behavior
- real SCCM input and vehicle-signal plausibility testing
- popup priority/dismiss behavior and repeated door-open edge testing
- native SDL audio open/play/close behavior through the GE board's ALSA/PipeWire route
- audible timing/level validation for warning high→low ×3 cadence and the slow door reminder
- audio-device disconnect/reconnect and output-stall fault injection

## Recording-matched click-free chime revision

After replacing the audible pitch/cadence with the user's measured 440/550/660 Hz recordings and adding raised-cosine note releases:

- `src/lib/chime_audio.cpp` passes the project's strict warning set with `-Werror` using the SDL declaration shim available in this environment.
- Release core verification remains **8/8 passing**.
- The audio callback remains allocation-free and lock-free.
- Note endpoints reach zero amplitude instead of truncating an active oscillator, eliminating the observed inter-note clicks.

## Start-of-stream and release-audio cleanup revision

- `src/lib/chime_audio.cpp` passes the full strict project warning set with `-Werror` after adding the 200 ms startup warm-up gate.
- The SDL callback is started immediately and remains active on digital zero; warning/door events arriving during the first 200 ms are held until the warm-up boundary.
- Release core verification remains **8/8 passing** in Release mode.
- The release tree contains exactly two audio reference files, both click-free recording-matched WAVs under `reference/chimes/`.
- Both reference WAVs were verified to contain exactly zero-valued PCM throughout the first 200 ms and final 150 ms.
- Older experimental/reconstructed WAV files are not included in the release.

## Typography / PRNDL follow-up

- Runtime text paths were audited after the visual-restoration revision.
- All HMI text now resolves exclusively to `FordAntennaCondWGL-Light.otf` or `FordAntennaCondWGL-Reg.otf`; traffic-sign text no longer selects a separate face.
- PRNDL inactive ranges use Antenna Light; the selected range uses Antenna Regular (bold/emphasis path) and Ford orange `#FF9600`.
- Strict Release core tests: **8/8 passed**.
- ASan+UBSan core tests: **8/8 passed**.
- SDL2 development libraries are not installed in this container, so target UI linking remains a GE-board build-time verification step.
