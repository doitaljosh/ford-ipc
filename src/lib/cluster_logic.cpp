#include "ford_ipc/cluster_logic.hpp"
#include "ford_ipc/time.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace fordadas {

VehiclePanelVisual panelTarget(const BodyStatus& b) noexcept {
    return {
        b.driver_front_ajar ? 1.0 : 0.0,
        b.passenger_front_ajar ? 1.0 : 0.0,
        b.rear_left_ajar ? 1.0 : 0.0,
        b.rear_right_ajar ? 1.0 : 0.0,
        b.hood_ajar ? 1.0 : 0.0,
        b.liftgate_ajar ? 1.0 : 0.0
    };
}

VehiclePanelVisual interpolatePanels(const BodyStatus& from, const BodyStatus& to, double t) noexcept {
    const auto a = panelTarget(from);
    const auto b = panelTarget(to);
    const double k = smooth01(t);
    auto mix = [k](double x, double y) noexcept { return x + (y - x) * k; };
    return {mix(a.driver_front,b.driver_front), mix(a.passenger_front,b.passenger_front),
            mix(a.rear_left,b.rear_left), mix(a.rear_right,b.rear_right),
            mix(a.hood,b.hood), mix(a.liftgate,b.liftgate)};
}

const VehiclePanelVisual& VehicleAnimator::update(const BodyStatus& b, double now) noexcept {
    const auto target = panelTarget(b);
    if (!initialized_) {
        value_ = target;
        last_ = now;
        initialized_ = true;
        return value_;
    }
    const double dt = clampd(now - last_, 0.0, 0.1);
    last_ = now;
    const double k = 1.0 - std::exp(-dt * 9.0);
    auto step = [k](double& v, double targetValue) noexcept {
        v += (targetValue - v) * k;
        if (std::abs(v - targetValue) < 0.001) v = targetValue;
    };
    step(value_.driver_front, target.driver_front);
    step(value_.passenger_front, target.passenger_front);
    step(value_.rear_left, target.rear_left);
    step(value_.rear_right, target.rear_right);
    step(value_.hood, target.hood);
    step(value_.liftgate, target.liftgate);
    return value_;
}

namespace {
bool sameBody(const BodyStatus& a, const BodyStatus& b) noexcept {
    return a.driver_front_ajar == b.driver_front_ajar &&
           a.passenger_front_ajar == b.passenger_front_ajar &&
           a.rear_left_ajar == b.rear_left_ajar &&
           a.rear_right_ajar == b.rear_right_ajar &&
           a.hood_ajar == b.hood_ajar &&
           a.liftgate_ajar == b.liftgate_ajar;
}

DoorPopupDetail newlyOpenedDoorDetail(const BodyStatus& a, const BodyStatus& b) noexcept {
    struct Item { bool before; bool after; DoorPopupDetail detail; };
    const std::array<Item,4> items{{
        {a.driver_front_ajar,b.driver_front_ajar,DoorPopupDetail::DriverDoor},
        {a.passenger_front_ajar,b.passenger_front_ajar,DoorPopupDetail::PassengerDoor},
        {a.rear_left_ajar,b.rear_left_ajar,DoorPopupDetail::LeftRearDoor},
        {a.rear_right_ajar,b.rear_right_ajar,DoorPopupDetail::RightRearDoor}
    }};
    DoorPopupDetail result = DoorPopupDetail::Generic;
    unsigned opened = 0U;
    for (const auto& i : items) {
        if (i.before || !i.after) continue;
        ++opened;
        if (opened == 1U) result = i.detail;
    }
    return opened > 1U ? DoorPopupDetail::MultipleDoors : result;
}

bool anyPassengerDoorOpen(const BodyStatus& b) noexcept {
    return b.driver_front_ajar || b.passenger_front_ajar ||
           b.rear_left_ajar || b.rear_right_ajar;
}
}

void PopupManager::enqueue(const PopupEvent& e) {
    if (queue_size_ >= QueueCapacity) {
        ++dropped_events_;
        if (e.priority < 3) return;
        queue_size_ = QueueCapacity - 1U; // reserve one slot for the higher-priority event
    }
    if (e.priority >= 3) {
        for (std::size_t i = queue_size_; i > 0U; --i) queue_[i] = queue_[i - 1U];
        queue_[0] = e;
        ++queue_size_;
    } else {
        queue_[queue_size_++] = e;
    }
}

void PopupManager::rising(bool value, bool& previous, const PopupEvent& e) {
    if (value && !previous) enqueue(e);
    previous = value;
}

void PopupManager::advance(double now) {
    if (active_ && now >= end_time_) active_.reset();
    if (!active_ && queue_size_ > 0U) {
        active_ = queue_[0];
        for (std::size_t i = 1U; i < queue_size_; ++i) queue_[i - 1U] = queue_[i];
        --queue_size_;
        start_time_ = now;
        end_time_ = now + active_->duration;
        if (active_sequence_ != std::numeric_limits<std::uint64_t>::max()) ++active_sequence_;
    }
}

bool PopupManager::update(const ADASState& s, double now) {
    bool consumed = false;
    advance(now);
    if (!ok_initialized_) {
        seen_ok_ = s.ok_press_count;
        ok_initialized_ = true;
    } else if (s.ok_press_count != seen_ok_) {
        seen_ok_ = s.ok_press_count;
        if (active_) {
            end_time_ = std::min(end_time_, now + 0.16);
            consumed = true;
        }
    }

    if (s.body.updated > 0.0) {
        if (!body_initialized_) {
            body_initialized_ = true;
            BodyStatus closed{};
            if (anyPassengerDoorOpen(s.body)) {
                enqueue({PopupKind::DoorAjar, DoorPopupDetail::Generic, closed, s.body, 5.0, 3});
            }
            last_body_ = s.body;
        } else if (!sameBody(last_body_, s.body)) {
            const DoorPopupDetail opened = newlyOpenedDoorDetail(last_body_, s.body);
            if (opened != DoorPopupDetail::Generic) {
                PopupEvent e{};
                e.kind = PopupKind::DoorAjar;
                e.door_detail = opened;
                e.body_before = last_body_;
                e.body_after = s.body;
                e.duration = 5.0;
                e.priority = 3;
                enqueue(e);
            }
            last_body_ = s.body;
        }
    }

    if (!warning_initialized_) warning_initialized_ = true;
    rising(s.warnings.full_accessory_power, prev_accessory_, {PopupKind::AccessoryPower,DoorPopupDetail::Generic,{},{},4.0,1});
    rising(s.warnings.washer_fluid_low, prev_washer_, {PopupKind::WasherFluid,DoorPopupDetail::Generic,{},{},5.0,2});
    rising(s.warnings.see_manual, prev_see_manual_, {PopupKind::SeeManual,DoorPopupDetail::Generic,{},{},5.0,2});
    rising(s.warnings.powertrain_malfunction, prev_powertrain_, {PopupKind::Powertrain,DoorPopupDetail::Generic,{},{},6.0,3});
    rising(s.warnings.engine_over_temperature, prev_over_temperature_, {PopupKind::EngineOverTemperature,DoorPopupDetail::Generic,{},{},6.0,3});
    rising(s.telltales.oil_pressure, prev_oil_pressure_, {PopupKind::EngineOilPressureLow,DoorPopupDetail::Generic,{},{},6.0,3});
    rising(s.warnings.charging_system_fault, prev_charging_, {PopupKind::ChargingSystem,DoorPopupDetail::Generic,{},{},6.0,3});
    advance(now);
    return consumed;
}

PopupVisual PopupManager::visual(double now) const {
    PopupVisual v{};
    if (!active_) return v;
    v.active = true;
    v.event = *active_;
    v.age = std::max(0.0, now - start_time_);
    v.sequence = active_sequence_;
    const double fadeIn = clampd(v.age / 0.22, 0.0, 1.0);
    const double fadeOut = clampd((end_time_ - now) / 0.22, 0.0, 1.0);
    v.alpha = static_cast<std::uint8_t>(std::lround(255.0 * smooth01(std::min(fadeIn, fadeOut))));
    return v;
}

void PopupManager::testPopup(PopupKind kind) {
    PopupEvent e{};
    e.kind = kind;
    const bool yellow = kind == PopupKind::AccessoryPower || kind == PopupKind::WasherFluid ||
                        kind == PopupKind::SeeManual;
    e.duration = (kind == PopupKind::AccessoryPower) ? 4.0 : (yellow ? 5.0 : 6.0);
    e.priority = (kind == PopupKind::AccessoryPower) ? 1 : (yellow ? 2 : 3);
    enqueue(e);
}

void TripComputer::reset() noexcept {
    distance_km = 0.0;
    elapsed_s = 0.0;
    speed_integral_kmh_s = 0.0;
}

double TripComputer::averageSpeedKph() const noexcept {
    return elapsed_s > 0.5 ? speed_integral_kmh_s / elapsed_s : 0.0;
}

void TripManager::update(const ADASState& s, double now, bool allowOkReset) noexcept {
    if (last_update_ <= 0.0) {
        last_update_ = now;
        seen_ok_count_ = s.ok_press_count;
        return;
    }
    const double dt = clampd(now - last_update_, 0.0, 0.25);
    last_update_ = now;
    if (s.ok_press_count != seen_ok_count_) {
        seen_ok_count_ = s.ok_press_count;
        if (allowOkReset) {
            if (s.center_view == CenterView::TripA) a_.reset();
            else if (s.center_view == CenterView::TripB) b_.reset();
        }
    }
    const bool speedFresh = s.vehicle.speed_kph.has_value() && s.freshness.isFresh(Decoder::ID_VEHICLE_SPEED, now, 1.0);
    const bool rpmFresh = s.vehicle.rpm.has_value() && s.freshness.isFresh(Decoder::ID_ENGINE_RPM, now, 1.0);
    const double speedKph = speedFresh ? std::max(0.0, *s.vehicle.speed_kph) : 0.0;
    const bool running = (rpmFresh && *s.vehicle.rpm > 300.0) || speedKph > 0.5;
    if (!running || dt <= 0.0) return;
    auto step = [dt, speedKph](TripComputer& trip) noexcept {
        trip.elapsed_s += dt;
        trip.distance_km += speedKph * dt / 3600.0;
        trip.speed_integral_kmh_s += speedKph * dt;
    };
    step(a_);
    step(b_);
}

StartupPostVisual startupPostVisual(double t) noexcept {
    StartupPostVisual p{};
    constexpr double End = 2.55;
    if (t < 0.0 || t >= End) return p;
    p.active = true;
    if (t < 0.18) {
        p.sweep_fraction = 0.0;
        p.telltale_alpha = static_cast<std::uint8_t>(255.0 * smooth01(t / 0.18));
    } else if (t < 1.08) {
        p.sweep_fraction = smooth01((t - 0.18) / 0.90);
        p.telltale_alpha = 255U;
    } else if (t < 1.28) {
        p.sweep_fraction = 1.0;
        p.telltale_alpha = 255U;
    } else if (t < 2.28) {
        p.sweep_fraction = 1.0 - smooth01((t - 1.28) / 1.00);
        const double fade = 1.0 - smooth01((t - 1.70) / 0.58);
        p.telltale_alpha = static_cast<std::uint8_t>(255.0 * clampd(fade, 0.0, 1.0));
    } else {
        p.sweep_fraction = 0.0;
        const double fade = 1.0 - smooth01((t - 2.28) / 0.27);
        p.telltale_alpha = static_cast<std::uint8_t>(255.0 * clampd(fade, 0.0, 1.0));
    }
    return p;
}

PageTransition::PageTransition(CenterView initial) noexcept : visible_(initial), target_(initial) {}

void PageTransition::request(CenterView desired, double now) noexcept {
    if (phase_ == Phase::Idle) {
        if (desired == visible_) return;
        target_ = desired;
        phase_ = Phase::FadeOut;
        phase_start_ = now;
        return;
    }
    if (desired == target_) return;
    target_ = desired;
    if (phase_ == Phase::FadeIn && desired != visible_) {
        phase_ = Phase::FadeOut;
        phase_start_ = now;
    }
}

PageTransitionVisual PageTransition::visual(double now) noexcept {
    constexpr double Half = 0.160;
    PageTransitionVisual v{visible_, 255U, phase_ != Phase::Idle};
    if (phase_ == Phase::Idle) return v;
    const double p = clampd((now - phase_start_) / Half, 0.0, 1.0);
    if (phase_ == Phase::FadeOut) {
        v.alpha = static_cast<std::uint8_t>(255.0 * (1.0 - smooth01(p)));
        if (p >= 1.0) {
            visible_ = target_;
            phase_ = Phase::FadeIn;
            phase_start_ = now;
            v.view = visible_;
            v.alpha = 0U;
        }
    } else {
        v.view = visible_;
        v.alpha = static_cast<std::uint8_t>(255.0 * smooth01(p));
        if (p >= 1.0) {
            phase_ = Phase::Idle;
            v.alpha = 255U;
            v.active = false;
        }
    }
    return v;
}

} // namespace fordadas
