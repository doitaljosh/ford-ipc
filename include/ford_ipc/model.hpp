#pragma once

#include "ford_ipc/fixed_text.hpp"

#include <array>
#include <cstddef>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <type_traits>

namespace fordadas {

struct LeadTarget {
    bool valid = false;
    double distance_m = 0.0;
    double lateral_m = 0.0;
    double rel_long_mps = 0.0;
    double rel_lat_mps = 0.0;
    std::optional<double> ttc_s;
    int confidence = 0;
    int obj_class = 0;
    double updated = 0.0;
};

struct SideStatus {
    int blind_detect = 0;
    int blind_alert = 0;
    int blind_sensor = 0;
    int blind_system = 0;
    bool cta_alert = false;
    int cta_alert_level = 0;
    int cta_sensor = 0;
    int cta_system = 0;
    double updated = 0.0;
};

enum class SpeedUnit : std::uint8_t { Unknown = 0, Kph, Mph };
enum class SignRestriction : std::uint8_t { None = 0, Wet, Snow, Trailer, Time };
enum class OvertakingRestriction : std::uint8_t {
    None = 0, NoPassing, NoPassingWet, NoPassingSnow, NoPassingTrailer, NoPassingTime
};

struct TrafficSigns {
    std::optional<int> speed_limit_1;
    std::optional<int> speed_limit_2;
    SpeedUnit unit = SpeedUnit::Unknown;
    SignRestriction restriction = SignRestriction::None;
    OvertakingRestriction overtaking = OvertakingRestriction::None;
    int tsr_status = 0;
    bool osw_warning = false;
    double updated = 0.0;
};

struct LaneState {
    double curvature = 0.0;
    double reference_angle_mrad = 0.0;
    int departure_state = 0;
    int departure_intensity = 0;
    int lka_state = 0;
    double path_offset_m = 0.0;
    double path_angle_rad = 0.0;
    std::optional<double> path_curvature;
    double updated = 0.0;
};

struct VehicleStatus {
    std::optional<double> speed_kph;
    std::optional<double> rpm;
    std::optional<int> gear;
    std::optional<double> dte_km;
    std::optional<double> outside_c;
    std::optional<double> odometer_km;
    std::optional<double> heading_deg;
    std::optional<int> compass_dir;
};

struct Telltales {
    int mil = 0;              // 0 off, 1 on, 2 flash
    bool oil_pressure = false;
    int abs_lamp = 0;         // 0 off, 1 on, 2 slow flash, 3 fast flash
    bool abs_fault = false;
    int traction_lamp = 0;    // 0 off, 1 on, 2 slow flash, 3 fast flash
    int brake_fluid = 0;      // 0 OK, 1 low, 2 unknown, 3 invalid
};

struct WarningStatus {
    bool full_accessory_power = false;
    bool washer_fluid_low = false;
    bool see_manual = false;
    bool powertrain_malfunction = false;
    bool engine_over_temperature = false;
    bool charging_system_fault = false;
    int ignition_status = 0;
    int low_voltage_fault_state = 0;
    double updated = 0.0;
};

enum class CenterView : std::uint8_t {
    ADAS = 0,
    TripA = 1,
    TripB = 2,
    NowPlaying = 3,
    Vehicle = 4,
    Count = 5
};

inline CenterView nextCenterView(CenterView v) noexcept {
    const auto n = static_cast<unsigned>(CenterView::Count);
    const auto x = (static_cast<unsigned>(v) + 1U) % n;
    return static_cast<CenterView>(x);
}

inline CenterView previousCenterView(CenterView v) noexcept {
    const auto n = static_cast<unsigned>(CenterView::Count);
    const auto x = (static_cast<unsigned>(v) + n - 1U) % n;
    return static_cast<CenterView>(x);
}

struct BodyStatus {
    bool driver_front_ajar = false;
    bool passenger_front_ajar = false;
    bool rear_left_ajar = false;
    bool rear_right_ajar = false;
    bool hood_ajar = false;
    bool liftgate_ajar = false;
    double updated = 0.0;
};

struct MediaInfo {
    FixedText<96> title;
    FixedText<64> artist;
    FixedText<80> album;
    FixedText<64> device;
    std::optional<std::uint32_t> track_number;
    std::optional<std::uint16_t> track_count;
    double updated = 0.0;
};

// Fixed-capacity freshness table: deterministic storage, no heap allocation in
// the CAN decode path, and no accidental insertion of arbitrary CAN IDs.
class SignalFreshness {
public:
    static constexpr std::size_t Capacity = 32U;

    bool mark(std::uint32_t id, double now) noexcept {
        for (auto& e : entries_) {
            if (e.used && e.id == id) {
                e.timestamp = now;
                return true;
            }
        }
        for (auto& e : entries_) {
            if (!e.used) {
                e.used = true;
                e.id = id;
                e.timestamp = now;
                return true;
            }
        }
        overflow_ = true;
        return false;
    }

    bool seen(std::uint32_t id) const noexcept {
        for (const auto& e : entries_) if (e.used && e.id == id) return true;
        return false;
    }

    bool isFresh(std::uint32_t id, double now, double timeout) const noexcept {
        for (const auto& e : entries_) {
            if (e.used && e.id == id) return now >= e.timestamp && (now - e.timestamp) <= timeout;
        }
        return false;
    }

    double timestamp(std::uint32_t id) const noexcept {
        for (const auto& e : entries_) if (e.used && e.id == id) return e.timestamp;
        return 0.0;
    }

    bool overflowed() const noexcept { return overflow_; }

private:
    struct Entry {
        std::uint32_t id = 0U;
        double timestamp = 0.0;
        bool used = false;
    };
    std::array<Entry, Capacity> entries_{};
    bool overflow_ = false;
};

enum class IntegrityState : std::uint8_t { Nominal = 0, Degraded = 1, SafeState = 2 };

struct ADASState {
    LaneState lane;
    LeadTarget lead;
    TrafficSigns signs;
    SideStatus left;
    SideStatus right;
    VehicleStatus vehicle;
    Telltales telltales;
    WarningStatus warnings;
    BodyStatus body;
    MediaInfo media;
    int follow_gap = 0;
    bool follow_mode = false;
    bool follow_gap_display = false;
    CenterView center_view = CenterView::ADAS;
    std::uint64_t ok_press_count = 0U;
    std::uint64_t frame_count = 0U;
    double frames_per_sec = 0.0;
    FixedText<48> mode_text{"DEMO"};
    FixedText<192> last_error;
    IntegrityState integrity_state = IntegrityState::Nominal;
    std::uint32_t integrity_faults = 0U;
    double last_frame_time = 0.0;
    SignalFreshness freshness;
};

static_assert(std::is_nothrow_copy_constructible<ADASState>::value,
              "ADASState snapshots must not throw during copy construction");
static_assert(std::is_nothrow_copy_assignable<ADASState>::value,
              "ADASState snapshots must not throw during copy assignment");

class SharedState {
public:
    ADASState snapshot() const {
        std::lock_guard<std::timed_mutex> lock(mu_);
        return state_;
    }

    bool trySnapshot(ADASState& out) const noexcept {
        try {
            std::unique_lock<std::timed_mutex> lock(mu_, std::defer_lock);
            if (!lock.try_lock_for(std::chrono::milliseconds(5))) return false;
            out = state_;
            return true;
        } catch (...) {
            return false;
        }
    }

    template <typename Fn>
    bool tryMutate(Fn&& fn) noexcept {
        try {
            std::unique_lock<std::timed_mutex> lock(mu_, std::defer_lock);
            if (!lock.try_lock_for(std::chrono::milliseconds(5))) return false;
            fn(state_);
            return true;
        } catch (...) {
            return false;
        }
    }

    template <typename Fn>
    void mutate(Fn&& fn) {
        std::lock_guard<std::timed_mutex> lock(mu_);
        fn(state_);
    }

private:
    mutable std::timed_mutex mu_;
    ADASState state_{};
};

} // namespace fordadas
