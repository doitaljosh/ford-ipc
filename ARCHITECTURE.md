# Modular architecture

The project is split into narrow software units. The hardened default links the protocol, safety, CAN, core-logic, and UI libraries **statically** into `ford_adas_ipc`; `FORD_ADAS_STATIC_CORE=OFF` remains available for development builds that specifically need shared core libraries.

## Dependency graph

```text
ford_adas_ipc
  |-- ford_can -------- SocketCAN + candump replay transport
  |      |-- ford_protocol
  |      `-- cluster_safety
  |-- cluster_ui ------ SDL2/GLES rendering, Ford assets/fonts + local chime output
  |      |-- cluster_core
  |      `-- ford_protocol
  |-- cluster_core ---- trips, popups, page fade, vehicle animation, demo/media bridge
  |      `-- ford_protocol
  |-- cluster_safety -- heartbeats, latched faults, plausibility, fail-silent policy
  |      `-- ford_protocol
  `-- ford_protocol --- Ford bit extraction, CAN IDs, signal decoders, bounded shared state
```

`FORD_ADAS_BUILD_UI=OFF` removes SDL/FreeType/GPU dependencies entirely and builds the protocol, safety, transport, core logic, and tests only. This is the preferred software-unit verification configuration.

## Source layout

- `include/ford_ipc/fixed_text.hpp` — fixed-capacity, always-NUL-terminated shared-state text.
- `include/ford_ipc/model.hpp` — bounded state model, signal freshness, timed shared-state access.
- `include/ford_ipc/bitfield.hpp` — reusable Motorola/DBC extraction helpers.
- `include/ford_ipc/ford_decoder.hpp` + `src/lib/ford_decoder.cpp` — Ford protocol decoder.
- `include/ford_ipc/can_transport.hpp` + `src/lib/can_transport.cpp` — receive-only SocketCAN/replay transport with CAN error subscription and liveness reporting.
- `include/ford_ipc/safety.hpp` + `src/lib/safety.cpp` — fault classification, watchdogs, plausibility checks, stale-data invalidation, and safe-state policy.
- `include/ford_ipc/cluster_logic.hpp` + `src/lib/cluster_logic.cpp` — bounded popup queue, opening-edge door alerts, Trip A/B, page transitions, startup POST, and vehicle-panel animation.
- `include/ford_ipc/app_services.hpp` + `src/lib/app_services.cpp` — demo and optional media metadata bridge.
- `include/ford_ipc/ui.hpp` + `src/lib/ui.cpp` — Ford IPC rendering plus allocation-free emergency fallback frame.
- `include/ford_ipc/chime_audio.hpp` + `src/lib/chime_audio.cpp` — isolated SDL audio callback for the reconstructed local Ford-style warning/door chime; no runtime audio-file dependency and no locks/allocations in the callback.
- `include/ford_ipc/app_config.hpp` + `src/lib/app_config.cpp` — host/application configuration.
- `src/app/main.cpp` — composition/event loop and safety reaction integration.

## Porting boundary

Ford protocol decoding, liveness supervision, safety reactions, stale-data behavior, SCCM navigation, trip logic, warnings, and popup logic can be reused on Raspberry Pi, GE Genio, or another target without carrying SDL/GPU code into the safety-core unit tests.
