#include "ford_ipc/ford_decoder.hpp"
#include "ford_ipc/safety.hpp"

#include "test_support.hpp"
#include <iostream>
#include <string>

using namespace fordadas;

int main() {
    ADASState s{};
    s.vehicle.speed_kph = 100.0;
    s.vehicle.rpm = 2500.0;
    s.follow_gap = 3;
    FORD_REQUIRE(validateSnapshotPlausibility(s));

    s.vehicle.speed_kph = 500.0;
    FORD_REQUIRE(!validateSnapshotPlausibility(s));
    s.vehicle.speed_kph = 100.0;
    s.follow_gap = 7;
    FORD_REQUIRE(!validateSnapshotPlausibility(s));
    s.follow_gap = 3;

    SafetyMonitor monitor;
    monitor.heartbeatRender(1000U);
    monitor.setCanExpected(true);
    monitor.heartbeatCanThread(1000U);
    monitor.heartbeatCanData(1000U);
    auto h = monitor.evaluate(1100U);
    FORD_REQUIRE(h.render_alive && h.can_thread_alive && h.can_data_alive);
    FORD_REQUIRE(h.mode == SafetyMode::Normal);

    h = monitor.evaluate(2600U);
    FORD_REQUIRE(!h.can_data_alive);
    FORD_REQUIRE(h.mode == SafetyMode::SafeState);

    SafetyMonitor degraded;
    degraded.heartbeatRender(1000U);
    degraded.report(SafetyFault::DecoderInput);
    FORD_REQUIRE(safetyModeForFaults(degraded.latchedFaults()) == SafetyMode::Degraded);
    FORD_REQUIRE(safetyFaultText(degraded.latchedFaults()).find("DECODER_INPUT") != std::string::npos);

    SafetyMonitor severe;
    severe.heartbeatRender(1000U);
    severe.report(SafetyFault::CanTransport);
    FORD_REQUIRE(safetyModeForFaults(severe.latchedFaults()) == SafetyMode::SafeState);

    SafetyMonitor audioOnly;
    audioOnly.heartbeatRender(1000U);
    audioOnly.report(SafetyFault::AudioOutput);
    FORD_REQUIRE(safetyModeForFaults(audioOnly.latchedFaults()) == SafetyMode::Degraded);
    FORD_REQUIRE(safetyFaultText(audioOnly.latchedFaults()).find("AUDIO_OUTPUT") != std::string::npos);

    ADASState stale{};
    stale.vehicle.speed_kph = 80.0;
    stale.vehicle.rpm = 2000.0;
    stale.freshness.mark(Decoder::ID_VEHICLE_SPEED, 1.0);
    stale.freshness.mark(Decoder::ID_ENGINE_RPM, 1.0);
    applySafetyDisplayPolicy(stale, 3.0, true, SafetyMode::Normal, 0U);
    FORD_REQUIRE(!stale.vehicle.speed_kph.has_value());
    FORD_REQUIRE(!stale.vehicle.rpm.has_value());

    ADASState failed{};
    failed.vehicle.speed_kph = 80.0;
    failed.vehicle.rpm = 2000.0;
    failed.lead.valid = true;
    failed.lead.distance_m = 30.0;
    failed.follow_gap = 3;
    applySafetyDisplayPolicy(failed, 2.0, true, SafetyMode::SafeState,
                             faultMask(SafetyFault::CanTransport));
    FORD_REQUIRE(failed.integrity_state == IntegrityState::SafeState);
    FORD_REQUIRE(!failed.vehicle.speed_kph.has_value());
    FORD_REQUIRE(!failed.vehicle.rpm.has_value());
    FORD_REQUIRE(!failed.lead.valid);
    FORD_REQUIRE(failed.follow_gap == 0);

    std::cout << "safety tests passed\n";
    return 0;
}
