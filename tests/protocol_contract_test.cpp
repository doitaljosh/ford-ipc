#include "ford_ipc/ford_decoder.hpp"
#include "test_support.hpp"
#include <iostream>
#include <set>

using namespace fordadas;

int main() {
    const auto& ids = Decoder::supportedIds();
    std::set<std::uint32_t> unique(ids.begin(), ids.end());
    FORD_REQUIRE(unique.size() == ids.size());
    FORD_REQUIRE(SignalFreshness::Capacity > ids.size());
    for (const auto id : ids) {
        FORD_REQUIRE(id <= 0x7FFU);
        FORD_REQUIRE(Decoder::isSupportedId(id));
    }
    std::cout << "protocol contract tests passed\n";
    return 0;
}
