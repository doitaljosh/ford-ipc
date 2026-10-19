#pragma once

#include <array>
#include <cstddef>
#include <ostream>
#include <string_view>

namespace fordadas {

// Fixed-capacity UTF-8 byte container for runtime state shared with safety-related
// code. It never allocates, is always NUL terminated, and truncates explicitly.
template <std::size_t Capacity>
class FixedText {
    static_assert(Capacity >= 2U, "FixedText requires room for data plus NUL");
public:
    constexpr FixedText() noexcept = default;

    FixedText(const char* value) noexcept { assign(value == nullptr ? std::string_view{} : std::string_view{value}); }
    FixedText(std::string_view value) noexcept { assign(value); }

    FixedText& operator=(const char* value) noexcept {
        assign(value == nullptr ? std::string_view{} : std::string_view{value});
        return *this;
    }
    FixedText& operator=(std::string_view value) noexcept {
        assign(value);
        return *this;
    }

    bool assign(std::string_view value) noexcept {
        const std::size_t n = value.size() < (Capacity - 1U) ? value.size() : (Capacity - 1U);
        for (std::size_t i = 0U; i < n; ++i) data_[i] = value[i];
        data_[n] = '\0';
        for (std::size_t i = n + 1U; i < Capacity; ++i) data_[i] = '\0';
        size_ = n;
        return n == value.size();
    }

    bool assignJoined(std::string_view first, std::string_view second) noexcept {
        clear();
        bool complete = append(first);
        complete = append(second) && complete;
        return complete;
    }

    bool append(std::string_view value) noexcept {
        const std::size_t room = (Capacity - 1U) - size_;
        const std::size_t n = value.size() < room ? value.size() : room;
        for (std::size_t i = 0U; i < n; ++i) data_[size_ + i] = value[i];
        size_ += n;
        data_[size_] = '\0';
        return n == value.size();
    }

    void clear() noexcept {
        data_.fill('\0');
        size_ = 0U;
    }

    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0U; }
    [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
    [[nodiscard]] constexpr std::size_t capacity() const noexcept { return Capacity - 1U; }
    [[nodiscard]] const char* c_str() const noexcept { return data_.data(); }
    [[nodiscard]] std::string_view view() const noexcept { return std::string_view{data_.data(), size_}; }
    constexpr operator std::string_view() const noexcept { return std::string_view{data_.data(), size_}; }

private:
    std::array<char, Capacity> data_{};
    std::size_t size_ = 0U;
};

template <std::size_t N>
inline bool operator==(const FixedText<N>& lhs, std::string_view rhs) noexcept { return lhs.view() == rhs; }
template <std::size_t N>
inline bool operator==(std::string_view lhs, const FixedText<N>& rhs) noexcept { return lhs == rhs.view(); }
template <std::size_t N>
inline bool operator!=(const FixedText<N>& lhs, std::string_view rhs) noexcept { return !(lhs == rhs); }
template <std::size_t N>
inline std::ostream& operator<<(std::ostream& os, const FixedText<N>& value) { return os << value.view(); }

} // namespace fordadas
