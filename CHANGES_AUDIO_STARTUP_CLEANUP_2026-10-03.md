# Audio startup-pop and release cleanup — 2026-10-03

- Preserved the recording-matched 440/550/660 Hz Ford chime tones and existing click-free note envelopes.
- Added a 200 ms runtime audio-device warm-up. SDL audio is opened once, immediately runs continuously on digital zero, and chimes cannot begin until the warm-up boundary.
- Added 200 ms digital-silence preroll and 150 ms postroll to the listen-only reference WAVs.
- Removed all superseded audio preview files from the release tree, including the earlier ACM-coefficient reconstructions, pre-click-fix previews, and five-pair analysis WAV.
- The release now contains exactly two audio reference files:
  - `reference/chimes/ford_door_chime_clickfree.wav`
  - `reference/chimes/ford_warning_chime_clickfree.wav`
- The runtime still synthesizes audio directly and does not load either WAV.
