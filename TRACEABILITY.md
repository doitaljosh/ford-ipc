# Provisional software-safety traceability matrix

These are repository engineering requirements, not a replacement for software safety requirements derived from an approved HARA/technical safety concept.

| ID | Repository requirement | Implementation | Verification |
|---|---|---|---|
| SSR-001 | Unsupported, malformed, non-8-byte, RTR, error, or extended CAN traffic shall not update vehicle state. | `ford_decoder.cpp`, `can_transport.cpp` | `decoder_robustness_test`, `protocol_contract_test` |
| SSR-002 | Accepted Ford CAN IDs and kernel filters shall derive from one canonical definition. | `Decoder::supportedIds()` | `protocol_contract_test` |
| SSR-003 | The application CAN transport shall be receive-only. | `can_transport.cpp` | source review/static analysis |
| SSR-004 | Implausible dynamic state shall be rejected or cause a latched plausibility fault. | `ford_decoder.cpp`, `safety.cpp` | `vehicle_decoder_test`, `safety_test` |
| SSR-005 | Stale live values shall not continue to be presented as current values. | `SignalFreshness`, `applySafetyDisplayPolicy()` | `safety_test`, trip tests |
| SSR-006 | CAN freshness storage shall be bounded and allocation-free. | `SignalFreshness` fixed array | robustness/contract tests |
| SSR-007 | SCCM navigation and OK input shall be edge-triggered. | `Decoder::steeringWheel()` | `ui_decoder_test` |
| SSR-008 | Popup scheduling shall be bounded and queue overflow shall be detectable. | `PopupManager`, `SafetyFault::PopupOverflow` | `cluster_logic_test` |
| SSR-009 | Render thread, CAN thread, and accepted-CAN-data flow shall be independently supervised. | `SafetyMonitor`, `SafetyWatchdog`, `SocketCANReceiver` | `safety_test` + target disconnect test |
| SSR-010 | Required HMI resources shall fail initialization rather than silently substitute misleading resources. | `AssetStore`, `TextCache` | target resource fault injection |
| SSR-011 | Protocol/safety/core software units shall build without SDL/GPU dependencies. | `FORD_ADAS_BUILD_UI=OFF` | `run_safety_checks.sh` |
| SSR-012 | Hardened builds shall support static protocol/safety/CAN/core linkage. | `FORD_ADAS_STATIC_CORE=ON` | build inspection |
| SSR-013 | Safety faults shall latch and map to Normal/Degraded/SafeState reactions. | `SafetyFault`, `safetyModeForFaults()` | `safety_test` |
| SSR-014 | Safe-state faults shall invalidate dynamic values rather than preserve plausible stale values. | `applySafetyDisplayPolicy()` | `safety_test` |
| SSR-015 | Shared runtime text used by the decoder/core shall be bounded and heap-free. | `FixedText<N>` | `fixed_text_test` |
| SSR-016 | Shared-state access failure shall be returned/reported explicitly. | `SharedState::trySnapshot/tryMutate`, `DecodeResult::StateAccessFailure` | source review + decoder error path |
| SSR-017 | CAN receiver and watchdog thread-creation failures shall be explicit latched faults. | `ReceiverStart`, `WatchdogStart` | source review + fault-injection integration test |
| SSR-018 | Core compiler diagnostics shall reject implicit narrowing/signedness and other configured warning classes. | CMake `COMMON_WARNINGS`, `FORD_ADAS_WERROR` | clean strict build |
| SSR-019 | Core sanitizer build shall complete with no ASan/UBSan findings in the unit test set. | `run_safety_checks.sh` | sanitizer CTest log |
| SSR-020 | Live shared-state access shall have a bounded wait and report timeout rather than block indefinitely. | `SharedState::trySnapshot/tryMutate` | strict unit build + source review |
| SSR-021 | CAN controller bus-off/control/restart errors shall be surfaced as transport faults. | `SocketCANReceiver` CAN error filter | source review + target bus-off fault injection |
| SSR-022 | Runtime HMI resource/render failure shall transition to a minimal resource-independent emergency presentation. | `ClusterUI::renderEmergencyFrame`, `main.cpp` | strict HMI syntax build + target resource fault injection |
| SSR-023 | Verification checks shall remain active in Release/NDEBUG builds. | `tests/test_support.hpp` / `FORD_REQUIRE` | strict Release CTest run |

## Evidence still required on target

Record, under configuration control, CAN disconnect/reconnect, no-traffic, bus-off, malformed traffic, process starvation, watchdog timing, GPU reset, display loss, memory pressure, missing/corrupt resources, target ELF loading/alignment, long-duration operation, input bounce, startup/shutdown, and power interruption tests. Production use also requires formal safety requirements and coverage/confirmation evidence outside this repository.

## Audible-warning extension

| ID | Software safety / HMI requirement | Implementation | Verification |
| --- | --- | --- | --- |
| SSR-024 | A new non-door warning popup shall request the configured warning chime exactly once per popup activation. | `PopupVisual::sequence`, `FordChimeAudio::triggerWarning()` | `cluster_logic_test`, integration review |
| SSR-025 | A passenger-door-open condition shall request a slow audible reminder and shall stop when all passenger doors close. | `FordChimeAudio::setDoorOpen()`, `notifyDoorOpened()` | callback design review, bench F8-F11 |
| SSR-026 | Warning/error chimes shall take priority over the door reminder without blocking visual rendering. | callback arbitration in `chime_audio.cpp` | code review, strict compile |
| SSR-027 | Loss of local audio output shall not suppress visual warnings or force invalid vehicle values. | `SafetyFault::AudioOutput` classified Degraded | `safety_test` |
| SSR-028 | Audio callback execution shall perform no dynamic allocation or mutex acquisition. | `FordChimeAudio::renderAudio()` | code review, sanitizer build |
