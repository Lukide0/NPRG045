#pragma once

#include <cassert>
#include <concepts>
#include <cstdint>
#include <limits>

/**
 * @brief Optional unsigned integer stored with a sentinel bit.
 *
 * @tparam T Unsigned integer type.
 */
template <std::unsigned_integral T> class optional_uint {
private:
    static constexpr T none_bit = 1 << (sizeof(T) * 8 - 1);

public:
    /**
     * @brief Creates an empty value.
     */
    static constexpr optional_uint none() { return optional_uint(none_bit); }

    /**
     * @brief Creates a valid value.
     *
     * @param value Stored integer value.
     */
    static constexpr optional_uint some(T value) {
        assert((value & none_bit) == 0);
        return optional_uint(value);
    }

    /**
     * @brief Checks whether a value is present.
     */
    [[nodiscard]] constexpr bool is_value() const { return (m_value & none_bit) == 0; }

    /**
     * @brief Checks whether the value is empty.
     */
    [[nodiscard]] constexpr bool is_none() const { return !is_value(); }

    /**
     * @brief Retrieves stored value.
     *
     * @return Stored unsigned integer.
     */
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
