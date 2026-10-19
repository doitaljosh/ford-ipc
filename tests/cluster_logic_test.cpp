#include "ford_ipc/cluster_logic.hpp"
#include "test_support.hpp"
#include <iostream>

using namespace fordadas;

int main() {
    PageTransition transition(CenterView::ADAS);
    transition.request(CenterView::TripA, 1.0);
    auto v = transition.visual(1.0);
    FORD_REQUIRE(v.active && v.view == CenterView::ADAS && v.alpha == 255U);
    v = transition.visual(1.17);
    FORD_REQUIRE(v.view == CenterView::TripA);
    v = transition.visual(1.34);
    FORD_REQUIRE(v.view == CenterView::TripA && v.alpha == 255U);

    PopupManager popups;
    for (int i = 0; i < 20; ++i) popups.testPopup(PopupKind::AccessoryPower);
    FORD_REQUIRE(popups.droppedEvents() > 0U);

    // Door popups are opening-edge only. A close event must stay silent.
    PopupManager doors;
    ADASState body{};
    body.body.updated = 1.0;
    doors.update(body, 1.0);
    FORD_REQUIRE(!doors.visual(1.0).active);
    body.body.driver_front_ajar = true;
    body.body.updated = 1.1;
    doors.update(body, 1.1);
    auto doorPopup = doors.visual(1.2);
    FORD_REQUIRE(doorPopup.active && doorPopup.event.kind == PopupKind::DoorAjar);
    FORD_REQUIRE(doorPopup.sequence != 0U);
    FORD_REQUIRE(doorPopup.event.body_after.driver_front_ajar);
    ++body.ok_press_count;
    doors.update(body, 1.21);
    doors.update(body, 1.5);
    body.body.driver_front_ajar = false;
    body.body.updated = 1.6;
    doors.update(body, 1.6);
    FORD_REQUIRE(!doors.visual(1.6).active);
    body.body.passenger_front_ajar = true;
    body.body.updated = 1.7;
    doors.update(body, 1.7);
    const auto secondDoorPopup = doors.visual(1.8);
    FORD_REQUIRE(secondDoorPopup.active);
    FORD_REQUIRE(secondDoorPopup.sequence > doorPopup.sequence);

    // Verified oil-pressure request is a red popup trigger.
    PopupManager oil;
    ADASState oilState{};
    oil.update(oilState, 2.0);
    oilState.telltales.oil_pressure = true;
    oil.update(oilState, 2.1);
    FORD_REQUIRE(oil.visual(2.2).event.kind == PopupKind::EngineOilPressureLow);

    TripManager trips;
    ADASState s{};
    s.center_view = CenterView::TripA;
    s.vehicle.speed_kph = 100.0;
    s.vehicle.rpm = 2000.0;
    s.freshness.mark(Decoder::ID_VEHICLE_SPEED, 2.0);
    s.freshness.mark(Decoder::ID_ENGINE_RPM, 2.0);
    trips.update(s, 2.0);
    trips.update(s, 2.1);
    FORD_REQUIRE(trips.a().distance_km > 0.0);
    ++s.ok_press_count;
    s.vehicle.speed_kph = 0.0;
    s.vehicle.rpm = 0.0;
    trips.update(s, 2.2);
    FORD_REQUIRE(trips.a().distance_km == 0.0);

    std::cout << "cluster logic tests passed\n";
    return 0;
}
