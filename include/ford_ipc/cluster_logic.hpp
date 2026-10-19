#pragma once

#include "ford_ipc/ford_decoder.hpp"
#include "ford_ipc/model.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace fordadas {

struct VehiclePanelVisual {
    double driver_front = 0.0;
    double passenger_front = 0.0;
    double rear_left = 0.0;
    double rear_right = 0.0;
    double hood = 0.0;
    double liftgate = 0.0;
};

VehiclePanelVisual panelTarget(const BodyStatus& b) noexcept;
VehiclePanelVisual interpolatePanels(const BodyStatus& from, const BodyStatus& to, double t) noexcept;

class VehicleAnimator {
public:
    const VehiclePanelVisual& update(const BodyStatus& b, double now) noexcept;
private:
    VehiclePanelVisual value_{};
    double last_ = 0.0;
    bool initialized_ = false;
};

enum class PopupKind : std::uint8_t {
    DoorAjar,
    AccessoryPower,
    WasherFluid,
    SeeManual,
    Powertrain,
    EngineOverTemperature,
    EngineOilPressureLow,
    ChargingSystem
};

enum class DoorPopupDetail : std::uint8_t {
    Generic = 0,
    DriverDoor,
    PassengerDoor,
    LeftRearDoor,
    RightRearDoor,
    MultipleDoors
};

struct PopupEvent {
    PopupKind kind = PopupKind::DoorAjar;
    DoorPopupDetail door_detail = DoorPopupDetail::Generic;
    BodyStatus body_before{};
    BodyStatus body_after{};
    double duration = 3.6;
    int priority = 1;
};

struct PopupVisual {
    bool active = false;
    PopupEvent event{};
    std::uint8_t alpha = 0U;
    double age = 0.0;
    std::uint64_t sequence = 0U;
};

class PopupManager {
public:
    bool update(const ADASState& s, double now);
    PopupVisual visual(double now) const;
    bool active() const noexcept { return active_.has_value(); }
    void testPopup(PopupKind kind);
    std::size_t droppedEvents() const noexcept { return dropped_events_; }

private:
    static constexpr std::size_t QueueCapacity = 8U;
    std::array<PopupEvent, QueueCapacity> queue_{};
    std::size_t queue_size_ = 0U;
    std::optional<PopupEvent> active_;
    double start_time_ = 0.0;
    double end_time_ = 0.0;
    std::uint64_t seen_ok_ = 0U;
    bool ok_initialized_ = false;
    bool body_initialized_ = false;
    bool warning_initialized_ = false;
    BodyStatus last_body_{};
    bool prev_accessory_ = false;
    bool prev_washer_ = false;
    bool prev_see_manual_ = false;
    bool prev_powertrain_ = false;
    bool prev_over_temperature_ = false;
    bool prev_oil_pressure_ = false;
    bool prev_charging_ = false;
    std::size_t dropped_events_ = 0U;
    std::uint64_t active_sequence_ = 0U;

    void enqueue(const PopupEvent& e);
    void rising(bool value, bool& previous, const PopupEvent& e);
    void advance(double now);
};

struct TripComputer {
    double distance_km = 0.0;
    double elapsed_s = 0.0;
    double speed_integral_kmh_s = 0.0;
    void reset() noexcept;
    double averageSpeedKph() const noexcept;
};

class TripManager {
public:
    void update(const ADASState& s, double now, bool allowOkReset = true) noexcept;
    const TripComputer& a() const noexcept { return a_; }
    const TripComputer& b() const noexcept { return b_; }
private:
    TripComputer a_{};
    TripComputer b_{};
    double last_update_ = 0.0;
    std::uint64_t seen_ok_count_ = 0U;
};

struct StartupPostVisual {
    bool active = false;
    double sweep_fraction = 0.0;
    std::uint8_t telltale_alpha = 0U;
};

StartupPostVisual startupPostVisual(double t) noexcept;

struct PageTransitionVisual {
    CenterView view = CenterView::ADAS;
    std::uint8_t alpha = 255U;
    bool active = false;
};

class PageTransition {
public:
    explicit PageTransition(CenterView initial = CenterView::ADAS) noexcept;
    void request(CenterView desired, double now) noexcept;
    PageTransitionVisual visual(double now) noexcept;
private:
    enum class Phase : std::uint8_t { Idle, FadeOut, FadeIn };
    CenterView visible_ = CenterView::ADAS;
    CenterView target_ = CenterView::ADAS;
    Phase phase_ = Phase::Idle;
    double phase_start_ = 0.0;
};

} // namespace fordadas
