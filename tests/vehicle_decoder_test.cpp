#include "ford_ipc/ford_decoder.hpp"
#include "test_support.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>

using namespace fordadas;

static void be_set(uint8_t* data, int start_bit, int bit_len, uint32_t value) {
    int pos = start_bit;
    for (int i = 0; i < bit_len; ++i) {
        const int byte_index = pos / 8;
        const int bit_index = pos % 8;
        const uint8_t bit = (value >> (bit_len - 1 - i)) & 1U;
        if (bit != 0U) data[byte_index] = static_cast<uint8_t>(data[byte_index] | static_cast<uint8_t>(1U << bit_index));
        else data[byte_index] = static_cast<uint8_t>(data[byte_index] & static_cast<uint8_t>(~(1U << bit_index)));
        pos = (pos % 8 == 0) ? pos + 15 : pos - 1;
    }
}

static bool near(double a, double b, double eps=1e-6) { return std::fabs(a-b) < eps; }

int main() {
    SharedState state;
    Decoder decoder(state);
    uint8_t d[8]{};

    // 100.00 km/h, quality OK.
    be_set(d, 38, 2, 3);
    be_set(d, 55, 16, 10000);
    decoder.feed(Decoder::ID_VEHICLE_SPEED, d, 8, 1.0);
    auto s = state.snapshot();
    FORD_REQUIRE(s.vehicle.speed_kph && near(*s.vehicle.speed_kph, 100.0));

    std::fill(std::begin(d), std::end(d), 0);
    be_set(d, 28, 13, 1500); // x2 = 3000 rpm
    decoder.feed(Decoder::ID_ENGINE_RPM, d, 8, 1.1);
    s = state.snapshot();
    FORD_REQUIRE(s.vehicle.rpm && near(*s.vehicle.rpm, 3000.0));

    std::fill(std::begin(d), std::end(d), 0);
    be_set(d, 7, 4, 3); // Drive
    decoder.feed(Decoder::ID_PRNDL, d, 8, 1.2);
    s = state.snapshot();
    FORD_REQUIRE(s.vehicle.gear && *s.vehicle.gear == 3);

    std::fill(std::begin(d), std::end(d), 0);
    be_set(d, 39, 14, 1234); // 123.4 km
    decoder.feed(Decoder::ID_DTE, d, 8, 1.3);
    s = state.snapshot();
    FORD_REQUIRE(s.vehicle.dte_km && near(*s.vehicle.dte_km, 123.4));

    std::fill(std::begin(d), std::end(d), 0);
    be_set(d, 35, 2, 3);
    be_set(d, 49, 10, 612); // 25 C
    decoder.feed(Decoder::ID_AMBIENT_TEMP, d, 8, 1.4);
    s = state.snapshot();
    FORD_REQUIRE(s.vehicle.outside_c && near(*s.vehicle.outside_c, 25.0));

    std::fill(std::begin(d), std::end(d), 0);
    be_set(d, 31, 24, 123456); // 12345.6 km
    decoder.feed(Decoder::ID_ODOMETER, d, 8, 1.5);
    s = state.snapshot();
    FORD_REQUIRE(s.vehicle.odometer_km && near(*s.vehicle.odometer_km, 12345.6));

    std::fill(std::begin(d), std::end(d), 0);
    be_set(d, 31, 16, 31500); // 315.00 deg
    decoder.feed(Decoder::ID_GPS_HEADING, d, 8, 1.6);
    s = state.snapshot();
    FORD_REQUIRE(s.vehicle.heading_deg && near(*s.vehicle.heading_deg, 315.0));

    std::fill(std::begin(d), std::end(d), 0);
    be_set(d, 3, 2, 2);   // MIL flash
    be_set(d, 18, 1, 1);  // oil lamp
    decoder.feed(Decoder::ID_ENGINE_LAMPS, d, 8, 1.7);
    s = state.snapshot();
    FORD_REQUIRE(s.telltales.mil == 2 && s.telltales.oil_pressure);

    std::fill(std::begin(d), std::end(d), 0);
    be_set(d, 42, 2, 2); // traction slow flash
    be_set(d, 5, 1, 1);  // ABS fault
    be_set(d, 55, 2, 1); // ABS lamp on
    decoder.feed(Decoder::ID_ABS_STATUS, d, 8, 1.8);
    s = state.snapshot();
    FORD_REQUIRE(s.telltales.traction_lamp == 2 && s.telltales.abs_fault && s.telltales.abs_lamp == 1);

    std::fill(std::begin(d), std::end(d), 0);
    be_set(d, 17, 2, 1); // low brake fluid
    decoder.feed(Decoder::ID_BRAKE_STATUS, d, 8, 1.9);
    s = state.snapshot();
    FORD_REQUIRE(s.telltales.brake_fluid == 1);

    std::cout << "vehicle decoder tests passed\n";
    return 0;
}
