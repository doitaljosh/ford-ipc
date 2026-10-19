#pragma once

#include <cstdlib>
#include <iostream>

namespace ford_test {
[[noreturn]] inline void fail(const char* expression, const char* file, int line) noexcept {
    std::cerr << file << ':' << line << ": requirement failed: " << expression << '\n';
    std::abort();
}
} // namespace ford_test

#define FORD_REQUIRE(expr) \
    do { if (!(expr)) ::ford_test::fail(#expr, __FILE__, __LINE__); } while (false)
