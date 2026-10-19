# Ford warning-chime reverse engineering and GE fallback implementation

## What is actually present in the supplied IPC firmware

The supplied 2020 Escape IPC firmware (`LV4T-14C088-AJ/BJ`) was inspected down
through its VBF payloads and decompressed `ifs2.bin` / `bitmaps.bin` images.
The cluster software contains Ford/Visteon warning-chime control symbols and
model data, including:

- `SBUS_WARNING_PLAY_CHIME_CMD`
- `SBUS_STG_INFO_CHIME_CMD`
- `SBUS_STG_TSR_OVERSPEED_CHIME_CMD`
- `SBUS_STG_PARK_SLOT_CHIME_CMD`
- `warnings_component:warn_chime_type:hard_chime`
- `warnings_component:warn_chime_type:soft_chime`
- `warnings_component:warn_chime_type:info_chime`
- `warnings_component:warn_chime_type:low_fuel_chime`
- `warnings_component:warn_chime_type:cross_traffic_alert_chime`
- additional warning-specific chime IDs.

No valid RIFF/WAVE or Ogg Vorbis sound resource was found in those IPC payloads.
That is consistent with the 2020 Escape architecture: the IPC arbitrates warning
chimes and normally commands the ACM/DSP to render them over the vehicle audio
system. The IPC also has a fallback audible path when the main audio source is
not available.

Accordingly, this project does **not** claim that its PCM waveform was extracted
from an OEM Ford WAV. The cadence is implemented from the user's requested
behavior, and the local GE waveform is a procedural Ford-style bell fallback.

## Implemented GE behavior

### Yellow/red warning and error

Every newly displayed non-door warning popup triggers:

```text
HIGH  LOW     HIGH  LOW     HIGH  LOW
 |     |       |     |       |     |
0.00  0.30    0.95  1.25    1.90  2.20 s
```

Each strike is 250 ms. The complete descending pair is repeated three times.
A newly latched software safety/integrity fault requests the same warning
pattern.

### Door open

Any of the four passenger doors drives a slower reminder:

- first bell immediately on opening;
- one 420 ms bell every 1.35 seconds while at least one passenger door remains
  open;
- closing all four doors silences the reminder;
- opening an additional/reopened door restarts the slow cadence immediately;
- a warning/error chime preempts the door reminder, after which the door
  reminder resumes if a door is still open.

This deliberately follows the requested bench/HMI behavior. The Ford service
information for a stock 2020 Escape describes the production door-ajar chime as
a single tone for each opening edge; this project intentionally uses the slower
continuous reminder requested for the GE display.

## Synthesized waveform

The local fallback chime is generated in real time with SDL's audio callback.
It does not use SDL_mixer and does not load audio files at runtime.

Measurements from the user's 2020 Escape recordings establish an almost exact
4:5:6 tone family:

- base tone: **440 Hz**
- upper warning tone: **550 Hz**
- middle door tone: **660 Hz**
- warning sequence: **440 -> 550 Hz**
- door sequence: **440 -> 660 -> 550 Hz**
- nominal sample rate: 48 kHz
- output: float PCM, stereo (or the SDL-selected channel count)
- default level: 72%, configurable with `--chime-volume`

Each note uses a raised-cosine attack, exponential decay and raised-cosine
terminal release. Notes are allowed to overlap, but every note reaches digital
zero before its synthesis window ends, eliminating the previous truncation click.

The SDL output device is opened once and left running continuously on digital zero.
A **200 ms startup warm-up** is enforced before the first audible sample is allowed,
so a DAC/mixer wake-up transient cannot coincide with a warning tone.

## Failure containment

`FordChimeAudio` is isolated from CAN decoding, popup generation and safety
state. Audio is non-authoritative: the warning remains visible if audio fails.

If the SDL audio output cannot be opened or stops running, the application
latches `AUDIO_OUTPUT`. It is classified as `Degraded`, not `SafeState`, because
the visual warning path remains functional.

The SDL callback:

- allocates no memory;
- acquires no locks;
- reads only atomic requests from the main thread;
- uses callback-owned phase/timing state;
- gives warning/error patterns priority over the door reminder.

## User controls

```text
--chime-volume 0..100    Local chime volume; default 72
--mute-chimes            Disable the local GE chime renderer
```

Bench warning keys F1-F7 automatically exercise the warning chime. Door test
keys F8-F11 automatically exercise the slow door cadence.


## Reference listening previews

The release intentionally contains only the two current click-free listen-only WAVs
under `reference/chimes/`:

- `ford_door_chime_clickfree.wav`
- `ford_warning_chime_clickfree.wav`

Each contains 200 ms of zero-valued PCM before the first tone and 150 ms after the
last tail. The WAVs are not runtime dependencies and are not claimed to be carved OEM
audio; they are recording-matched procedural references. All older experimental,
763/918 Hz, and pre-click-fix WAVs are excluded from the release.

## ACM firmware findings

The supplied `JX7T-14C302-BM` ACM application contains parameterized chime/envelope
profiles but no conventional WAV/Ogg assets. Earlier analysis tentatively interpreted
two internal coefficient pairs as audible pitches near 763/918 Hz. The user's vehicle
recordings disprove that interpretation for the audible warning/door tones. Those
coefficients remain useful reverse-engineering evidence for an internal filter or
resonator stage, but the GE renderer uses the measured **440/550/660 Hz** tone family.
See `ACM_CHIME_REVERSE_ENGINEERING.md` and `CHIME_RECORDING_MATCH_2026-10-03.md`.
