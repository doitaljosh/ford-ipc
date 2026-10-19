# 2020 Escape ACM chime reverse-engineering findings

> **Recording-based correction (2026-10-03):** Subsequent clean recordings from the actual vehicle establish audible chime tones at **440, 550 and 660 Hz**. The earlier 763/918 Hz mapping of the coefficient pairs below is therefore not the audible pitch mapping. Those pairs may belong to an internal filter/resonator stage. The GE runtime now uses the measured 440/550/660 Hz tones and measured cadence.


Source examined: user-supplied `JX7T-14C302-BM.VBF` (Ford Gen 4.1 PWB-B Premium ACM application), with comparison to `JX7T-14C302-GA.VBF` and `JX7T-14C305-BD.VBF`.

## Bottom line

The cleaner ACM warning chimes are **not stored as WAV/Ogg/MP3 audio assets** in the supplied VBFs. The ACM application contains a fixed-size table of synthesized chime profiles. This fits Ford's documented architecture: the IPC arbitrates warning chimes and sends the ACM/DSP chime characteristics (priority, volume, tone timing, repetition, selection and routing); the audio module renders the sound.

Three consecutive 0x11C-byte records in `JX7T-14C302-BM.VBF` match Ford's documented message-center chime types exactly:

| Profile | VBF offset | Flash address | Tone length | 3 kHz segment counts |
|---|---:|---:|---:|---:|
| Information (ID 3) | `0xC5FD7` | `0xD1CC0` | 1000 ms | 30 + 2970 = 3000 |
| Soft warning (ID 4) | `0xC60F3` | `0xD1DDC` | 500 ms | 15 + 1485 = 1500 |
| Hard warning (ID 5) | `0xC620F` | `0xD1EF8` | 250 ms | 15 + 735 = 750 |

The exact 1000/500/250 ms sequence is the strongest identification evidence because Ford service documentation defines Information / Soft / Hard message-center warnings by those same lengths.

## Envelope parameters recovered

Each profile contains a 3,000 Hz control rate and a Q23 decay coefficient at record offset `+0x2C`:

- Information: `0x007FF514` = 0.9996666908
- Soft warning: `0x007FEA29` = 0.9993335009
- Hard warning: `0x007FD456` = 0.9986674786

These values are almost exactly `exp(-1/N)`, where N is the profile length in 3 kHz control ticks (3000, 1500, 750 respectively). That demonstrates that these fields are envelope/decay parameters rather than unrelated vehicle calibration data.

The common tone block also contains:

- `0x002F16AC` in Q23 = 0.36787939, effectively `e^-1`
- `0x0000106C` in Q23 = 0.00050116, approximately -66.0 dB
- exact reciprocal-like Q23 constants such as 1/120, 1/60 and 1/12

## Oscillator/resonator evidence

Profiles 3/4/5 share the same tone block. Two coefficient pairs are especially revealing:

- Primary: `sin = 0x0331` interpreted Q13, `cos = 0x07F6` interpreted Q11
- Secondary: `sin = 0x03D5` interpreted Q13, `cos = 0x07EE` interpreted Q11
- The block also contains `0xF800`, exact -1.0 in Q11.

The scaled sin/cos pairs lie very close to the unit circle. Using `atan2(sin, cos)` in the ACM's 48 kHz audio domain gives approximately:

- primary resonator: **763.08 Hz**
- secondary resonator: **918.49 Hz**

This is substantially stronger evidence than simply treating one coefficient as an arbitrary pitch number. It is consistent with a digital resonator/oscillator implementation. The precise proprietary SAF3600 state initialization/mix topology is not yet fully decoded, so the included WAVs are labeled **firmware-derived reconstructions**, not bit-exact extracted Ford audio.

## Why no OEM WAV was found

- No valid RIFF/WAVE, Ogg Vorbis, FLAC or MP3 audio payload was found in the ACM application/calibration supplied here.
- `JX7T-14C302-GA` is a high-entropy secondary image and does not expose a conventional sound-resource container.
- The BM application instead contains explicit CAN receive dispatching and these parameterized chime profile records.

This matches Ford's design: the IPC requests a chime; the ACM/DSP synthesizes/renders it.

## Included tools / outputs

`extract_acm_chime_profiles.py` locates the three records directly in a locally supplied `JX7T-14C302-BM.VBF`, exports each 0x11C-byte record, and emits CSV/JSON summaries. This avoids redistributing the original ACM firmware.

The WAV files in this findings package are generated from the recovered durations, 3 kHz decay coefficients and resonator estimates so they can be auditioned. They are not carved PCM resources.
