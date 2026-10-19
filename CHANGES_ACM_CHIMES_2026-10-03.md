# ACM-derived chime revision — 2026-10-03

- Reverse engineered the user-supplied `JX7T-14C302-BM.VBF` ACM application.
- Confirmed the ACM does not expose conventional WAV/Ogg warning-chime assets; it contains synthesized chime profile records.
- Located consecutive profile IDs 3/4/5 at VBF offsets `0xC5FD7`, `0xC60F3`, `0xC620F`.
- Their tone lengths are exactly 1000 / 500 / 250 ms, matching Ford Information / Soft-warning / Hard-warning definitions.
- Recovered the 3 kHz envelope-control rate and the Q23 decay coefficients for all three profiles.
- Recovered common resonator coefficient pairs. Interpreted as quantized Q13 sine / Q11 cosine terms in the 48 kHz audio domain, the two tone components are approximately 763.08 Hz and 918.49 Hz.
- Replaced the previous G5/E5 guessed local chime pitches and hand-tuned bell envelope with the ACM-derived resonator/timing/envelope parameters.
- Retained the requested warning cadence (high/low pair repeated three times) and slow door reminder cadence.
- Added `tools/extract_acm_chime_profiles.py` so the parameters can be reproduced from a locally supplied ACM VBF without redistributing Ford firmware.
- Added `ACM_CHIME_REVERSE_ENGINEERING.md` documenting offsets, raw constants, math and current confidence limits.
- Strict Release core verification: 8/8 tests passed.
- ASan+UBSan core verification: 8/8 tests passed.
