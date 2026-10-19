#include "ford_ipc/fixed_text.hpp"

#include "test_support.hpp"
#include <iostream>
#include <string_view>

using namespace fordadas;

int main() {
    FixedText<8> text{"abc"};
    FORD_REQUIRE(text.view() == "abc");
    FORD_REQUIRE(text.assign("1234567"));
    FORD_REQUIRE(text.view() == "1234567");
    FORD_REQUIRE(!text.assign("123456789"));
    FORD_REQUIRE(text.view() == "1234567");
    FORD_REQUIRE(!text.assignJoined("hello", "world"));
    FORD_REQUIRE(text.view() == "hellowo");
    text.clear();
    FORD_REQUIRE(text.empty());
    std::cout << "fixed text tests passed\n";
    return 0;
}
