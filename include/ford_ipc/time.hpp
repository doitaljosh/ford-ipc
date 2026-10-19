#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace fordadas {

using MonotonicMs = std::uint64_t;

inline MonotonicMs monotonicMilliseconds() noexcept {
    using namespace std::chrono;
    const auto value = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    return value < 0 ? 0U : static_cast<MonotonicMs>(value);
}

inline double monotonicSeconds() noexcept {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

inline double clampd(double v, double lo, double hi) noexcept {
    return std::max(lo, std::min(hi, v));
}

inline double smooth01(double t) noexcept {
    t = clampd(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

} // namespace fordadas
