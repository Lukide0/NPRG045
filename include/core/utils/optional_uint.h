#pragma once

#include <cassert>
#include <concepts>
#include <cstdint>
#include <limits>

template <std::unsigned_integral T> class optional_uint {
private:
    static constexpr T max      = std::numeric_limits<T>::max();
    static constexpr T none_bit = 1 << (sizeof(T) * 8 - 1);

public:
    static constexpr optional_uint none() { return optional_uint(none_bit); }

    static constexpr optional_uint some(T value) {
        assert((value & none_bit) == 0);
        return optional_uint(value);
    }

    [[nodiscard]] constexpr bool is_value() const { return (m_value & none_bit) == 0; }

    [[nodiscard]] constexpr bool is_none() const { return !is_value(); }

    [[nodiscard]] constexpr T value() const {
        assert(is_value());
        return m_value;
    }

private:
    constexpr optional_uint(T value)
        : m_value(value) { }

    T m_value;
};

using optional_u31 = optional_uint<std::uint32_t>;
using optional_u15 = optional_uint<std::uint16_t>;
using optional_u7  = optional_uint<std::uint8_t>;
