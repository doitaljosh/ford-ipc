#pragma once

#include <cstddef>
#include <cstdint>

namespace fordadas {

inline std::uint32_t be_raw(const std::uint8_t* data, std::size_t len, int start_bit, int bit_len) noexcept {
    if (data == nullptr || bit_len <= 0 || bit_len > 32 || start_bit < 0) return 0U;
    std::uint32_t value = 0U;
    int pos = start_bit;
    for (int i = 0; i < bit_len; ++i) {
        const int byte_index = pos / 8;
        const int bit_index = pos % 8;
        if (byte_index < 0 || static_cast<std::size_t>(byte_index) >= len) return 0U;
        value = (value << 1U) | ((data[byte_index] >> bit_index) & 1U);
        pos = (pos % 8 == 0) ? pos + 15 : pos - 1;
    }
    return value;
}

inline double be_phys(const std::uint8_t* data, std::size_t len, int start, int bits,
                      double scale = 1.0, double offset = 0.0) noexcept {
    return static_cast<double>(be_raw(data, len, start, bits)) * scale + offset;
}

} // namespace fordadas
