# GE visual restoration — 2026-10-03

- Restored the 360x360 supersampled gauge faces at native size instead of downscaling them to 344x344.
- Restored the original smooth gauge treatment: radial bowl shading, glass highlight, metallic bezel, soft outer shadow, blurred rotating needle shadow, specular needle and layered hub.
- Reduced the Driver Assist road/background from the oversized full-height trapezoid to a lane-aligned road surface.
- The road surface now begins at y=288 where the Ford lane assets become visibly opaque.
- Bottom corners terminate exactly at the center vertical dividers: x=390 and x=890 at y=660.
- Road sides begin at x=612/668 near the visible lane-line horizon and run to x=390/890 at the lower dividers, keeping the road edge within about 2 degrees of the Ford lane-line perspective.
- Added a subtle blue-black vertical road gradient and faint perspective-edge highlights rather than a flat black mass.
- Audio/chime, warning, door, CAN, safety and page logic are unchanged.
