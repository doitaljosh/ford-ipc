#include "ford_ipc/ford_decoder.hpp"
#include <algorithm>
#include <array>
#include "test_support.hpp"
#include <cstdint>
#include <iostream>

using namespace fordadas;

static void be_set(uint8_t* data, int start_bit, int bit_len, uint32_t value) {
    int pos = start_bit;
    for (int i = 0; i < bit_len; ++i) {
        const int byte_index = pos / 8;
        const int bit_index = pos % 8;
        const uint8_t bit = (value >> (bit_len - 1 - i)) & 1U;
        if (bit) data[byte_index] |= static_cast<uint8_t>(1U << bit_index);
        else data[byte_index] &= static_cast<uint8_t>(~(1U << bit_index));
        pos = (pos % 8 == 0) ? pos + 15 : pos - 1;
    }
}

int main() {
    SharedState state;
    Decoder decoder(state);
    uint8_t d[8]{};

    // ACCDATA_3 / 0x18A: selected time gap = 4, gap display active.
    be_set(d, 17, 1, 1);
    be_set(d, 35, 1, 1);
    be_set(d, 34, 3, 4);
    decoder.feed(Decoder::ID_ACCDATA_3, d, 8, 1.0);
    auto s = state.snapshot();
    FORD_REQUIRE(s.follow_mode);
    FORD_REQUIRE(s.follow_gap_display);
    FORD_REQUIRE(s.follow_gap == 4);

    // Steering_Data_FD1 / 0x083: SCCM/GWM gap-decrease switch event.  The
    // 0x18A display value remains authoritative when present, but this is a
    // useful immediate/fallback update path.
    std::fill(std::begin(d), std::end(d), 0);
    be_set(d, 12, 1, 1);
    decoder.feed(Decoder::ID_STEERING_DATA, d, 8, 1.05);
    s = state.snapshot();
    FORD_REQUIRE(s.follow_gap == 3);
    std::fill(std::begin(d), std::end(d), 0);
    decoder.feed(Decoder::ID_STEERING_DATA, d, 8, 1.06);

    // Steering_Wheel_Data2_FD1 / 0x081: Down advances on rising edge only.
    std::fill(std::begin(d), std::end(d), 0);
    be_set(d, 0, 1, 1);
    decoder.feed(Decoder::ID_STEERING_WHEEL, d, 8, 1.1);
    s = state.snapshot();
    FORD_REQUIRE(s.center_view == CenterView::TripA);
    decoder.feed(Decoder::ID_STEERING_WHEEL, d, 8, 1.2); // held: no repeat
    FORD_REQUIRE(state.snapshot().center_view == CenterView::TripA);
    std::fill(std::begin(d), std::end(d), 0);              // release
    decoder.feed(Decoder::ID_STEERING_WHEEL, d, 8, 1.3);
    be_set(d, 0, 1, 1);
    decoder.feed(Decoder::ID_STEERING_WHEEL, d, 8, 1.4);
    FORD_REQUIRE(state.snapshot().center_view == CenterView::TripB);

    // Up returns to previous page.
    std::fill(std::begin(d), std::end(d), 0);
    decoder.feed(Decoder::ID_STEERING_WHEEL, d, 8, 1.5);
    be_set(d, 3, 1, 1);
    decoder.feed(Decoder::ID_STEERING_WHEEL, d, 8, 1.6);
    FORD_REQUIRE(state.snapshot().center_view == CenterView::TripA);

    // OK has an edge counter used to reset Trip A or B.
    std::fill(std::begin(d), std::end(d), 0);
    decoder.feed(Decoder::ID_STEERING_WHEEL, d, 8, 1.7);
    be_set(d, 4, 1, 1);
    decoder.feed(Decoder::ID_STEERING_WHEEL, d, 8, 1.8);
    FORD_REQUIRE(state.snapshot().ok_press_count == 1);

    // BodyInfo_3_FD1 / 0x3B3: driver door, rear-left door, hood and liftgate ajar.
    std::fill(std::begin(d), std::end(d), 0);
    be_set(d, 61, 1, 1);
    be_set(d, 48, 1, 1);
    be_set(d, 59, 1, 1);
    be_set(d, 0, 1, 1);
    decoder.feed(Decoder::ID_BODY_INFO, d, 8, 2.0);
    s = state.snapshot();
    FORD_REQUIRE(s.body.driver_front_ajar);
    FORD_REQUIRE(!s.body.passenger_front_ajar);
    FORD_REQUIRE(s.body.rear_left_ajar);
    FORD_REQUIRE(!s.body.rear_right_ajar);
    FORD_REQUIRE(s.body.hood_ajar);
    FORD_REQUIRE(s.body.liftgate_ajar);

    // BodyInfo ignition/accessory state drives the Full Accessory Power popup.
    std::fill(std::begin(d), std::end(d), 0);
    be_set(d, 7, 4, 2); // Accessory
    decoder.feed(Decoder::ID_BODY_INFO, d, 8, 2.05);
    s = state.snapshot();
    FORD_REQUIRE(s.warnings.full_accessory_power);
    FORD_REQUIRE(s.warnings.ignition_status == 2);

    // Engine service request is the yellow wrench / See Manual request.
    std::fill(std::begin(d), std::end(d), 0);
    be_set(d, 19, 1, 1);
    decoder.feed(Decoder::ID_ENGINE_SERVICE, d, 8, 2.06);
    FORD_REQUIRE(state.snapshot().warnings.see_manual);

    // Low-voltage fault state 4 = no output -> Check Charging System.
    std::fill(std::begin(d), std::end(d), 0);
    be_set(d, 12, 4, 4);
    decoder.feed(Decoder::ID_LOW_VOLTAGE_PWR, d, 8, 2.07);
    s = state.snapshot();
    FORD_REQUIRE(s.warnings.low_voltage_fault_state == 4);
    FORD_REQUIRE(s.warnings.charging_system_fault);

    // APIM track index/count (text metadata is supplied by the optional metadata file).
    std::fill(std::begin(d), std::end(d), 0);
    be_set(d, 7, 32, 9);
    be_set(d, 39, 16, 14);
    decoder.feed(Decoder::ID_APIM_TRACK, d, 8, 2.1);
    s = state.snapshot();
    FORD_REQUIRE(s.media.track_number && *s.media.track_number == 9);
    FORD_REQUIRE(s.media.track_count && *s.media.track_count == 14);

    std::cout << "ui decoder tests passed\n";
    return 0;
}
