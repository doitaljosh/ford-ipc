#include "ford_ipc/ford_decoder.hpp"
#include "test_support.hpp"
#include <cstdint>
#include <iostream>

using namespace fordadas;

int main() {
    // Basic Motorola bit-walk checks independent of vehicle calibration.
    const uint8_t d[8] = {0b10110010, 0b01101100, 0xA5, 0x5A, 0, 0, 0, 0};
    FORD_REQUIRE(be_raw(d, 8, 7, 8) == 0xB2);
    FORD_REQUIRE(be_raw(d, 8, 15, 8) == 0x6C);

    SharedState state;
    Decoder decoder(state);
    uint8_t zero[8]{};
    decoder.feed(Decoder::ID_SIDE_LEFT, zero, 8, 1.0);
    auto s = state.snapshot();
    FORD_REQUIRE(s.frame_count == 1);
    FORD_REQUIRE(s.left.updated == 1.0);
    FORD_REQUIRE(s.freshness.seen(Decoder::ID_SIDE_LEFT));

    decoder.feed(Decoder::ID_TRAFFIC_RECOG, zero, 8, 2.0);
    s = state.snapshot();
    FORD_REQUIRE(s.frame_count == 2);
    FORD_REQUIRE(!s.signs.speed_limit_1.has_value());

    std::cout << "decoder tests passed\n";
    return 0;
}
