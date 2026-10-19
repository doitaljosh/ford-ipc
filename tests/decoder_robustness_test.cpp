#include "ford_ipc/ford_decoder.hpp"
#include <array>
#include "test_support.hpp"
#include <cstdint>
#include <iostream>

using namespace fordadas;

int main() {
    SharedState state;
    Decoder decoder(state);
    std::array<std::uint8_t, 8> bytes{};

    FORD_REQUIRE(decoder.feed(0x7FFU, bytes.data(), bytes.size(), 1.0) == DecodeResult::UnsupportedId);
    FORD_REQUIRE(decoder.feed(Decoder::ID_VEHICLE_SPEED, bytes.data(), 7U, 1.0) == DecodeResult::InvalidLength);
    FORD_REQUIRE(decoder.feed(Decoder::ID_VEHICLE_SPEED, nullptr, 8U, 1.0) == DecodeResult::InvalidLength);
    FORD_REQUIRE(decoder.feed(Decoder::ID_VEHICLE_SPEED, bytes.data(), 8U, -1.0) == DecodeResult::InvalidTimestamp);
    FORD_REQUIRE(state.snapshot().frame_count == 0U);

    // Every supported frame accepts a full 8-byte payload without escaping its
    // bounded decode path or corrupting the freshness table.
    for (const auto id : Decoder::supportedIds()) {
        const auto r = decoder.feed(id, bytes.data(), bytes.size(), 2.0);
        FORD_REQUIRE(r == DecodeResult::Accepted);
    }
    const auto snap = state.snapshot();
    FORD_REQUIRE(snap.frame_count == Decoder::SupportedIdCount);
    FORD_REQUIRE(!snap.freshness.overflowed());
    std::cout << "decoder robustness tests passed\n";
    return 0;
}
