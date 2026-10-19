# Recording-matched Ford chimes — 2026-10-03

## Measured audible tone family

The user supplied recordings from the 2020 Escape establish three nearly pure sine tones:

- 440 Hz
- 550 Hz
- 660 Hz

These form an exact 4:5:6 just-intonation major triad.

## Door reminder

Sequence: **440 -> 660 -> 550 Hz**.

- note-start spacing: ~153 ms
- complete group repeat: ~1.608 s
- decaying tails overlap

## Warning-message chime

Sequence: **440 -> 550 Hz**.

- note-start spacing: ~149 ms
- pair repeat: ~357 ms
- the supplied recording contains five pairs; the GE application intentionally retains the earlier requested three-pair warning event.

## Pop/click correction

The first reconstruction truncated each still-decaying oscillator at a fixed duration while its instantaneous sample was non-zero. That creates an impulse and was audible as a harsh pop between notes.

The runtime now uses:

1. raised-cosine attack;
2. exponential decay;
3. raised-cosine terminal release to exactly zero;
4. overlapping tails between successive notes.

The frequency and note timing are unchanged; only the discontinuity was removed.


## Start-of-stream pop correction

The click at the very beginning of otherwise click-free preview files was not a
frequency/envelope error: the generated PCM already began at exact digital zero.
It was consistent with the host player / DAC / mixer becoming active at the same
instant as the first audible note. The authoritative reference WAVs now include
**200 ms silent preroll** and **150 ms silent postroll**.

The GE runtime avoids the same condition by opening SDL audio once, immediately
running the callback on digital zero, and reserving the first **200 ms** as a
hardware/mixer warm-up. Chime events arriving during that window are delayed until
the warm-up boundary rather than starting the audio device on demand.

## ACM coefficient caveat

Earlier reverse engineering interpreted two fixed-point ACM coefficient pairs as audible resonator frequencies near 763 and 918 Hz. The vehicle recordings disprove those as the actual chime pitches. They may still be internal filter/resonator coefficients in another signal-processing domain, but the GE renderer no longer uses them for audible frequency.
