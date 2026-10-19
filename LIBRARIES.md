# Reusable software-unit APIs

The hardened build uses static core linkage by default. Names below refer to CMake targets rather than requiring `.so` runtime objects.

## `ford_protocol`

Headers: `ford_ipc/model.hpp`, `ford_ipc/fixed_text.hpp`, `ford_ipc/bitfield.hpp`, `ford_ipc/ford_decoder.hpp`

```cpp
fordadas::SharedState state;
fordadas::Decoder decoder(state);
auto result = decoder.feed(can_id, payload, 8, monotonic_time);
fordadas::ADASState snapshot{};
if (!state.trySnapshot(snapshot)) {
    // report a shared-state safety fault
}
```

`Decoder::supportedIds()` is the canonical ID list used by both protocol tests and the SocketCAN kernel filter set.

## `ford_can`

Header: `ford_ipc/can_transport.hpp`

Provides `SocketCANReceiver` and `ReplayReceiver`. Live CAN is receive-only, subscribes to CAN controller error frames, and reports both receiver-thread liveness and accepted-data liveness.

## `cluster_core`

Headers: `ford_ipc/cluster_logic.hpp`, `ford_ipc/app_services.hpp`

Reusable non-rendering HMI behavior: startup POST timing, page transitions, Trip A/B integration/reset, fixed-capacity popup manager, integrated door/hood/liftgate animation, demo source, and optional media bridge.

## `cluster_safety`

Header: `ford_ipc/safety.hpp`

Provides latched diagnostic faults, integer monotonic heartbeats, watchdog supervision, state plausibility checks, fault reaction classification, stale-value invalidation, and fail-silent snapshot policy.

## `cluster_ui`

Headers: `ford_ipc/ui.hpp`, `ford_ipc/chime_audio.hpp`

Owns SDL render targets, Ford assets, runtime Antenna font rendering, the cluster scene renderer, integrity overlays, a no-font/no-asset emergency fallback frame, and the isolated local Ford-style chime renderer. The chime callback is non-authoritative and has no runtime WAV/Ogg dependency.

## Headless verification build

```bash
cmake -S . -B build-core \
  -DFORD_ADAS_BUILD_UI=OFF \
  -DFORD_ADAS_STATIC_CORE=ON \
  -DFORD_ADAS_WERROR=ON
cmake --build build-core -j"$(nproc)"
ctest --test-dir build-core --output-on-failure
```
