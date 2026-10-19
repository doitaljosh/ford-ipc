# ASIL-B software-oriented engineering baseline

## Scope and claim

This source tree has been hardened as an **ISO 26262 ASIL-B-oriented software baseline**. It is **not, by itself, an ISO 26262 / ASIL-B compliant item or certified product**. ISO 26262-6:2018 covers software safety requirements, software architecture, unit design/implementation, unit verification, software integration/verification, and embedded-software testing; the wider ISO 26262 lifecycle also requires safety management, concept/HARA, system and hardware work products, supporting processes, confirmation measures, configuration/change control, and production evidence.

The code therefore implements safety mechanisms and verification hooks that are suitable inputs to an ASIL-B development process, while explicitly avoiding a false certification claim.

## Implemented software safety measures

### 1. Deterministic safety-relevant state

- `FixedText<N>` replaces heap-backed `std::string` fields in the shared runtime state.
- Live shared-state acquisition is bounded to 5 ms; failure is surfaced instead of allowing an unbounded wait.
- CAN-decoded state, warnings, body state, mode/error text, and media metadata can be copied without heap allocation.
- Popup door descriptions use a bounded enum rather than heap-backed strings.
- Signal freshness uses a fixed-capacity `std::array` and cannot grow at runtime.
- Popup scheduling uses a fixed-capacity queue.

### 2. Explicit fault model and reactions

`SafetyFault` is a latched bitmask with explicit faults for:

- decoder input rejection
- CAN transport failure
- CAN receive-thread heartbeat loss
- accepted-CAN-data timeout
- render heartbeat loss
- state plausibility failure
- resource integrity failure
- freshness-table exhaustion
- shared-state access failure
- monotonic-clock regression
- popup overflow
- watchdog thread start failure
- CAN/replay receiver thread start failure

Faults are classified into `Normal`, `Degraded`, or `SafeState` rather than being only logged.

### 3. Fail-silent display policy

When a safe-state fault is latched:

- speed and RPM are invalidated
- DTE, outside temperature, and heading are invalidated
- lead-target, lane/path, traffic-sign, BLIS/CTA, and following-gap dynamic data are invalidated
- stale live-CAN values are not retained as current values
- the HMI is instructed to present an unmistakable **VEHICLE DATA UNAVAILABLE** integrity overlay

The objective is to avoid a failure mode where a frozen value continues to look plausible.

### 4. Independent liveness supervision

The watchdog uses integer `steady_clock` milliseconds, not floating-point atomics. It independently supervises:

- render-loop heartbeat
- CAN receiver-thread heartbeat
- accepted CAN-data heartbeat

A CAN thread that is alive but receiving no accepted frames therefore does not satisfy the data-liveness requirement.

### 5. Defensive CAN handling

- The live SocketCAN path is receive-only.
- Supported CAN IDs come from one compile-time canonical list.
- Kernel CAN filters are generated from that same list.
- Extended, RTR, malformed-length, and unsupported frames do not update state.
- SocketCAN controller error frames for bus-off, controller-state faults, and restart are subscribed explicitly and latch a transport fault.
- Decoder return codes are explicit, including shared-state access failure.
- Receiver thread creation failure is reported explicitly.
- Physical plausibility checks cover speed, RPM, DTE, temperature, odometer, heading, gear, lane/path quantities, lead-target quantities, warning fields, and enum/range validity.

### 6. Safety/HMI separation

The decoder, safety monitor, CAN transport, and cluster core build and test without SDL, SDL_image, SDL_ttf, Mesa, or a GPU. `FORD_ADAS_BUILD_UI=OFF` is the software-unit verification build.

The hardened default is `FORD_ADAS_STATIC_CORE=ON`. This keeps the modular source boundaries while removing runtime `.so` loading for the protocol/safety/CAN/core libraries in the normal build.

### 7. Compiler and runtime hardening

The core and tests use strict warnings with `-Werror`, including conversion, signedness, cast-alignment, cast-qualification, switch-enum, format, shadowing, null-dereference and fallthrough diagnostics.

Hardening includes:

```text
-fstack-protector-strong
-fno-common
-fno-omit-frame-pointer
_GLIBCXX_ASSERTIONS=1
_FORTIFY_SOURCE=3 (Release)
-Wl,-z,relro
-Wl,-z,now
-Wl,-z,noexecstack
```

`-ffast-math` is intentionally not used.

### 8. Verification hooks

Core CTest currently covers:

- CAN decoder behavior
- vehicle signal decoding
- SCCM/UI control decoding
- malformed-input robustness
- protocol contract
- popup/trip logic
- safety classification, heartbeats, CAN-data timeout and fail-silent behavior
- fixed-capacity text truncation/termination

`run_safety_checks.sh` builds the core with ASan + UBSan and runs the complete core test set. Verification assertions use `FORD_REQUIRE` rather than C `assert()`, so they remain active in Release/NDEBUG builds. If installed, the script also runs cppcheck and clang-tidy.

If normal text/texture rendering fails at runtime, the HMI latches `ResourceIntegrity` and switches to an emergency SDL-primitives-only screen that does not depend on fonts or image assets.

## Safety assumptions that remain external to this repository

The following are still required before any credible ASIL-B compliance claim could be made for a production item:

- item definition and intended safety function
- HARA, safety goals, and ASIL assignment
- technical safety concept and system architecture
- derived software safety requirements approved under configuration control
- bidirectional requirement/design/code/test traceability in the organization’s safety process
- independence/confirmation reviews required by the selected lifecycle
- compiler/static-analysis/test-tool confidence or qualification arguments
- target hardware timing/WCET evidence
- structural coverage evidence appropriate to the selected verification methods
- target fault-injection campaign
- power-on/boot integrity and software authenticity mechanism
- hardware watchdog and supervised power/reset strategy
- freedom-from-interference argument for Linux/SDL/Mesa/FreeType/GPU components, or replacement with qualified/safety-element-out-of-context components
- CAN end-to-end protection checks where the Ford source messages actually provide counters/CRC/data IDs
- verified live mappings for warnings that remain test-only in this project
- production, service, diagnostic, calibration, and change-management processes

## Platform limitation

The GE/Ubuntu Linux platform, the Linux kernel, Mesa, SDL2, SDL_ttf/FreeType, the MediaTek/Panfrost GPU stack, libc/libstdc++, and the refrigerator display hardware are not qualified here as safety elements. Static core linkage reduces one runtime dependency class, but it does not turn the general-purpose OS/HMI stack into an ASIL-B platform.

## Intended use

Use this build for development, reverse engineering, bench integration, and safety-process prototyping. Do not remove the vehicle's production cluster or use this program as the sole safety-related driver information source without completing the missing ISO 26262 lifecycle and platform evidence.

## Audible-warning fault containment

The GE local chime renderer is an HMI supplement and is isolated from the
safety-relevant state/visual warning path. `AUDIO_OUTPUT` is a degraded fault:
loss of sound never causes the warning popup to disappear and never preserves
stale vehicle values. The SDL callback is allocation-free and lock-free and
uses atomic requests from the main thread. This is an implementation hardening
measure; overall audible-warning safety classification still belongs in the
vehicle-level safety concept/HARA.
