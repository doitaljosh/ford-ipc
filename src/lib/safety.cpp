#include "ford_ipc/safety.hpp"

#include "ford_ipc/ford_decoder.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>
#include <thread>

namespace fordadas {
namespace {

bool heartbeatAlive(MonotonicMs last, MonotonicMs now, MonotonicMs timeout) noexcept {
    return last != 0U && now >= last && (now - last) <= timeout;
}

bool finiteOptional(const std::optional<double>& v) noexcept {
    return !v.has_value() || std::isfinite(*v);
}

bool enumInRange(CenterView v) noexcept {
    return static_cast<unsigned>(v) < static_cast<unsigned>(CenterView::Count);
}

void invalidateIfStale(std::optional<double>& value, const SignalFreshness& freshness,
                       std::uint32_t id, double now, double timeout) noexcept {
    if (value.has_value() && !freshness.isFresh(id, now, timeout)) value.reset();
}

} // namespace

void SafetyMonitor::heartbeat(std::atomic<MonotonicMs>& target, MonotonicMs now) noexcept {
    const MonotonicMs old = target.load(std::memory_order_acquire);
    if (old != 0U && now < old) {
        report(SafetyFault::ClockRegression);
        return;
    }
    target.store(now, std::memory_order_release);
}

void SafetyMonitor::heartbeatRender(MonotonicMs now) noexcept { heartbeat(render_heartbeat_, now); }
void SafetyMonitor::heartbeatCanThread(MonotonicMs now) noexcept { heartbeat(can_thread_heartbeat_, now); }
void SafetyMonitor::heartbeatCanData(MonotonicMs now) noexcept { heartbeat(can_data_heartbeat_, now); }

void SafetyMonitor::report(SafetyFault fault) noexcept {
    faults_.fetch_or(faultMask(fault), std::memory_order_acq_rel);
}

SafetyMode safetyModeForFaults(std::uint32_t mask) noexcept {
    constexpr std::uint32_t SafeStateMask =
        faultMask(SafetyFault::CanTransport) |
        faultMask(SafetyFault::CanThreadHeartbeat) |
        faultMask(SafetyFault::CanDataTimeout) |
        faultMask(SafetyFault::RenderHeartbeat) |
        faultMask(SafetyFault::StatePlausibility) |
        faultMask(SafetyFault::ResourceIntegrity) |
        faultMask(SafetyFault::FreshnessTableFull) |
        faultMask(SafetyFault::SharedStateAccess) |
        faultMask(SafetyFault::ClockRegression) |
        faultMask(SafetyFault::WatchdogStart) |
        faultMask(SafetyFault::ReceiverStart);
    if ((mask & SafeStateMask) != 0U) return SafetyMode::SafeState;
    return mask == 0U ? SafetyMode::Normal : SafetyMode::Degraded;
}

SafetyHealth SafetyMonitor::evaluate(MonotonicMs now) const noexcept {
    SafetyHealth h{};
    h.latched_faults = faults_.load(std::memory_order_acquire);
    h.can_expected = can_expected_.load(std::memory_order_acquire);
    h.render_alive = heartbeatAlive(render_heartbeat_.load(std::memory_order_acquire), now, RenderTimeoutMs);
    h.can_thread_alive = !h.can_expected || heartbeatAlive(
        can_thread_heartbeat_.load(std::memory_order_acquire), now, CanThreadTimeoutMs);
    h.can_data_alive = !h.can_expected || heartbeatAlive(
        can_data_heartbeat_.load(std::memory_order_acquire), now, CanDataTimeoutMs);

    std::uint32_t effective = h.latched_faults;
    if (!h.render_alive) effective |= faultMask(SafetyFault::RenderHeartbeat);
    if (!h.can_thread_alive) effective |= faultMask(SafetyFault::CanThreadHeartbeat);
    if (!h.can_data_alive) effective |= faultMask(SafetyFault::CanDataTimeout);
    h.mode = safetyModeForFaults(effective);
    return h;
}

SafetyWatchdog::~SafetyWatchdog() { stop(); }

bool SafetyWatchdog::start() noexcept {
    if (started_.load(std::memory_order_acquire)) return true;
    stop_.store(false, std::memory_order_release);
    try {
        thread_ = std::thread([this] { run(); });
        started_.store(true, std::memory_order_release);
        return true;
    } catch (...) {
        monitor_.report(SafetyFault::WatchdogStart);
        return false;
    }
}

void SafetyWatchdog::stop() noexcept {
    stop_.store(true, std::memory_order_release);
    if (thread_.joinable()) thread_.join();
    started_.store(false, std::memory_order_release);
}

void SafetyWatchdog::run() noexcept {
    while (!stop_.load(std::memory_order_acquire)) {
        const auto h = monitor_.evaluate(monotonicMilliseconds());
        if (!h.render_alive) monitor_.report(SafetyFault::RenderHeartbeat);
        if (!h.can_thread_alive) monitor_.report(SafetyFault::CanThreadHeartbeat);
        if (!h.can_data_alive) monitor_.report(SafetyFault::CanDataTimeout);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool validateSnapshotPlausibility(const ADASState& s) noexcept {
    if (!finiteOptional(s.vehicle.speed_kph) || !finiteOptional(s.vehicle.rpm) ||
        !finiteOptional(s.vehicle.dte_km) || !finiteOptional(s.vehicle.outside_c) ||
        !finiteOptional(s.vehicle.odometer_km) || !finiteOptional(s.vehicle.heading_deg) ||
        !finiteOptional(s.lead.ttc_s)) return false;

    if (s.vehicle.speed_kph && (*s.vehicle.speed_kph < 0.0 || *s.vehicle.speed_kph > 300.0)) return false;
    if (s.vehicle.rpm && (*s.vehicle.rpm < 0.0 || *s.vehicle.rpm > 10000.0)) return false;
    if (s.vehicle.dte_km && (*s.vehicle.dte_km < 0.0 || *s.vehicle.dte_km > 2500.0)) return false;
    if (s.vehicle.outside_c && (*s.vehicle.outside_c < -60.0 || *s.vehicle.outside_c > 100.0)) return false;
    if (s.vehicle.odometer_km && (*s.vehicle.odometer_km < 0.0 || *s.vehicle.odometer_km > 2000000.0)) return false;
    if (s.vehicle.heading_deg && (*s.vehicle.heading_deg < 0.0 || *s.vehicle.heading_deg > 360.0)) return false;
    if (s.vehicle.gear && (*s.vehicle.gear < 0 || *s.vehicle.gear > 11)) return false;

    if (!std::isfinite(s.lane.curvature) || !std::isfinite(s.lane.reference_angle_mrad) ||
        !std::isfinite(s.lane.path_offset_m) || !std::isfinite(s.lane.path_angle_rad)) return false;
    if (std::abs(s.lane.path_offset_m) > 10.0 || std::abs(s.lane.path_angle_rad) > 1.0) return false;

    if (!std::isfinite(s.lead.distance_m) || !std::isfinite(s.lead.lateral_m) ||
        !std::isfinite(s.lead.rel_long_mps) || !std::isfinite(s.lead.rel_lat_mps)) return false;
    if (s.lead.valid && (s.lead.distance_m < 0.0 || s.lead.distance_m > 300.0 ||
                         std::abs(s.lead.lateral_m) > 20.0 ||
                         std::abs(s.lead.rel_long_mps) > 120.0 ||
                         std::abs(s.lead.rel_lat_mps) > 60.0)) return false;
    if (s.lead.ttc_s && (*s.lead.ttc_s < 0.0 || *s.lead.ttc_s > 120.0)) return false;

    if (s.follow_gap < 0 || s.follow_gap > 4) return false;
    if (!enumInRange(s.center_view)) return false;
    if (s.telltales.mil < 0 || s.telltales.mil > 2 || s.telltales.abs_lamp < 0 ||
        s.telltales.abs_lamp > 3 || s.telltales.traction_lamp < 0 || s.telltales.traction_lamp > 3 ||
        s.telltales.brake_fluid < 0 || s.telltales.brake_fluid > 3) return false;
    if (s.warnings.ignition_status < 0 || s.warnings.ignition_status > 15 ||
        s.warnings.low_voltage_fault_state < 0 || s.warnings.low_voltage_fault_state > 15) return false;

    const auto finiteStamp = [](double t) noexcept { return std::isfinite(t) && t >= 0.0; };
    if (!finiteStamp(s.last_frame_time) || !finiteStamp(s.body.updated) || !finiteStamp(s.warnings.updated) ||
        !finiteStamp(s.lane.updated) || !finiteStamp(s.lead.updated) || !finiteStamp(s.signs.updated) ||
        !finiteStamp(s.left.updated) || !finiteStamp(s.right.updated) || !finiteStamp(s.media.updated)) return false;
    if (!std::isfinite(s.frames_per_sec) || s.frames_per_sec < 0.0 || s.frames_per_sec > 10000.0) return false;
    if (s.freshness.overflowed()) return false;
    return true;
}

void applySafetyDisplayPolicy(ADASState& s, double now, bool live_can,
                              SafetyMode mode, std::uint32_t faults) noexcept {
    s.integrity_faults = faults;
    s.integrity_state = mode == SafetyMode::SafeState ? IntegrityState::SafeState :
                        (mode == SafetyMode::Degraded ? IntegrityState::Degraded : IntegrityState::Nominal);

    if (live_can && std::isfinite(now) && now >= 0.0) {
        invalidateIfStale(s.vehicle.speed_kph, s.freshness, Decoder::ID_VEHICLE_SPEED, now, 1.0);
        invalidateIfStale(s.vehicle.rpm, s.freshness, Decoder::ID_ENGINE_RPM, now, 1.0);
        invalidateIfStale(s.vehicle.dte_km, s.freshness, Decoder::ID_DTE, now, 3.0);
        invalidateIfStale(s.vehicle.outside_c, s.freshness, Decoder::ID_AMBIENT_TEMP, now, 5.0);
        invalidateIfStale(s.vehicle.odometer_km, s.freshness, Decoder::ID_ODOMETER, now, 5.0);
        invalidateIfStale(s.vehicle.heading_deg, s.freshness, Decoder::ID_GPS_HEADING, now, 3.0);
    }

    if (mode != SafetyMode::SafeState) return;

    // Fail-silent presentation: never preserve stale safety-relevant dynamic
    // values after an integrity fault. The dedicated safety overlay remains
    // visible, while values that could be mistaken for current measurements
    // are rendered unavailable.
    s.vehicle.speed_kph.reset();
    s.vehicle.rpm.reset();
    s.vehicle.dte_km.reset();
    s.vehicle.outside_c.reset();
    s.vehicle.heading_deg.reset();
    s.lead = LeadTarget{};
    s.lane = LaneState{};
    s.signs = TrafficSigns{};
    s.left = SideStatus{};
    s.right = SideStatus{};
    s.follow_gap = 0;
    s.follow_mode = false;
    s.follow_gap_display = false;
}

std::string safetyFaultText(std::uint32_t mask) {
    if (mask == 0U) return "OK";
    std::ostringstream out;
    auto add = [&](SafetyFault f, const char* name) {
        if ((mask & faultMask(f)) == 0U) return;
        if (out.tellp() > 0) out << ',';
        out << name;
    };
    add(SafetyFault::DecoderInput, "DECODER_INPUT");
    add(SafetyFault::CanTransport, "CAN_TRANSPORT");
    add(SafetyFault::CanThreadHeartbeat, "CAN_THREAD_HEARTBEAT");
    add(SafetyFault::CanDataTimeout, "CAN_DATA_TIMEOUT");
    add(SafetyFault::RenderHeartbeat, "RENDER_HEARTBEAT");
    add(SafetyFault::StatePlausibility, "STATE_PLAUSIBILITY");
    add(SafetyFault::ResourceIntegrity, "RESOURCE_INTEGRITY");
    add(SafetyFault::FreshnessTableFull, "FRESHNESS_TABLE");
    add(SafetyFault::SharedStateAccess, "SHARED_STATE_ACCESS");
    add(SafetyFault::ClockRegression, "CLOCK_REGRESSION");
    add(SafetyFault::PopupOverflow, "POPUP_OVERFLOW");
    add(SafetyFault::WatchdogStart, "WATCHDOG_START");
    add(SafetyFault::ReceiverStart, "RECEIVER_START");
    add(SafetyFault::AudioOutput, "AUDIO_OUTPUT");
    return out.str();
}

} // namespace fordadas
