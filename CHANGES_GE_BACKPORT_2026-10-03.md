# GE 1280×800 backport — 2026-10-03

This revision moves the current hardened Ford IPC branch back to the GE GEA-SBC001 refrigerator display while retaining the newer safety and feature work.

## Platform/HMI changes

- logical canvas changed from 1480×320 to **1280×800**
- default window changed to 1280×800
- portrait 800×1280 GE panel remains supported through final-frame auto-rotation
- restored the GE-style large left/right gauge placement
- expanded the center page region vertically instead of scaling the strip UI
- moved BLIS/CTA views into the GE lower-center flanks
- restored the GE full-width bottom status band
- enlarged popup panels, icons, warning text and bottom-right OK control
- enlarged integrated vehicle rendering for door animation and Vehicle page
- retained the newer green 1–4 ACC gap indication and omission of the small host vehicle below it
- retained native Ford OTF/TTF rendering; no font atlas reintroduced

## Safety/core preservation

No safety mechanism was intentionally removed for the backport. Decoder, CAN transport, popup state machine, fixed-capacity state, watchdog/heartbeat logic, stale-data handling, plausibility checks, safe-state policy and test suite remain the hardened implementations.
