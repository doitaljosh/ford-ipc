# Chime implementation — 2026-10-03

- Inspected the supplied `LV4T-14C088-AJ/BJ` IPC firmware payloads for warning
  chime resources and control strings.
- Confirmed `SBUS_WARNING_PLAY_CHIME_CMD` and Ford warning chime type enums are
  present; no valid WAV/Ogg resource was found in the IPC images.
- Added `FordChimeAudio`, a nonblocking SDL float-PCM fallback renderer for the
  GE GEA-SBC001.
- Warning/error cadence: descending two-tone pair repeated three times.
- Door cadence: slow repeated bell while any passenger door is open, restarted
  on each opening edge.
- Warning/error audio preempts door audio; door reminder resumes if still open.
- Added per-popup activation sequence numbers so sound triggers once for each
  displayed warning rather than once per render frame.
- Added `AUDIO_OUTPUT` as a degraded safety fault.
- Added `--chime-volume` and `--mute-chimes`.
- Added chime behavior and failure-containment documentation.
