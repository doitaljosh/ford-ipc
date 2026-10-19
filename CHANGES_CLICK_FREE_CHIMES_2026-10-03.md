# Click-free recording-matched chime revision — 2026-10-03

- Kept the measured 2020 Escape tone set: 440 / 550 / 660 Hz.
- Door group remains 440 -> 660 -> 550 Hz with ~153 ms between note starts and ~1.608 s between groups.
- Warning remains 440 -> 550 Hz with ~149 ms note spacing and ~357 ms pair spacing; this build retains the requested three pairs per warning event.
- Removed the audible clicks/pops caused by truncating a still-active sine at a non-zero sample.
- Each note now uses raised-cosine attack and release ramps, with a natural exponential decay and an explicit zero-amplitude terminal release.
- Overlapping tails are preserved, matching the smooth sound heard in the supplied vehicle recordings.

- Added a 200 ms continuous-silence audio-device warm-up before the first runtime chime, while keeping the device open thereafter.
- Added 200 ms silent preroll + 150 ms postroll to the two reference WAVs to prevent player/DAC start transients.
- Release audio cleanup: only `reference/chimes/ford_door_chime_clickfree.wav` and `reference/chimes/ford_warning_chime_clickfree.wav` are shipped; older experimental/reconstructed WAVs were removed.
