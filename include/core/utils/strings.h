#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

namespace core::utils {

namespace detail {

    template <typename T>
    concept str_impl = requires(const T& obj, std::size_t i) {
        { obj.ptr() } -> std::same_as<const char*>;
        { obj.size() } -> std::same_as<std::size_t>;
        { obj[i] } -> std::same_as<char>;
    };

    template <std::size_t Size> class str_array {
    public:
        constexpr str_array(std::array<char, Size> str)
            : m_str(str) { }

        constexpr char operator[](std::size_t i) const { return m_str[i]; }

        [[nodiscard]] constexpr std::size_t size() const { return Size - 1; }

        [[nodiscard]] constexpr const char* ptr() const { return m_str.data(); }

        constexpr operator const char*() const { return ptr(); }

        [[nodiscard]] constexpr const char* c_str() const { return ptr(); }

    private:
        std::array<char, Size> m_str;
    };

    template <std::size_t Size> class str_ref {
    public:
        constexpr str_ref(const char (&str)[Size])
            : m_str(str) { }

        constexpr char operator[](std::size_t i) const { return m_str[i]; }

        [[nodiscard]] consteval std::size_t size() const { return Size - 1; }

        [[nodiscard]] constexpr const char* ptr() const { return m_str; }

        constexpr operator const char*() const { return ptr(); }

        [[nodiscard]] constexpr const char* c_str() const { return ptr(); }

    private:
        const char (&m_str)[Size];
    };

    template <std::size_t S1, std::size_t S2> consteval auto concat(const str_impl auto& a, const str_impl auto& b) {
        std::array<char, S1 + S2 - 1> str;
        auto iter = str.begin();
        iter      = std::copy_n(a.ptr(), a.size(), iter);
        iter      = std::copy_n(b.ptr(), b.size(), iter);

        str.back() = '\0';

        return str_array<S1 + S2 - 1>(str);
    }

    template <std::size_t S1, std::size_t S2> consteval auto operator+(const str_array<S1>& a, const str_array<S2>& b) {
        return concat<S1, S2>(a, b);
    }

    template <std::size_t S1, std::size_t S2> consteval auto operator+(const str_ref<S1>& a, const str_ref<S2>& b) {
        return concat<S1, S2>(a, b);
    }

    template <std::size_t S1, std::size_t S2> consteval auto operator+(const str_array<S1>& a, const str_ref<S2>& b) {
        return concat<S1, S2>(a, b);
    }

    template <std::size_t S1, std::size_t S2> consteval auto operator+(const str_ref<S1>& a, const str_array<S2>& b) {
        return concat<S1, S2>(a, b);
    }

    template <std::size_t S1, std::size_t S2> consteval auto operator+(const str_ref<S1>& a, const char (&b)[S2]) {
        return concat<S1, S2>(a, str_ref<S2>(b));
    }

    template <std::size_t S1, std::size_t S2> consteval auto operator+(const char (&a)[S1], const str_ref<S2>& b) {
        return concat<S1, S2>(str_ref<S1>(a), b);
    }

    template <std::size_t S1, std::size_t S2> consteval auto operator+(const str_array<S1>& a, const char (&b)[S2]) {
        return concat<S1, S2>(a, str_ref<S2>(b));
    }

    template <std::size_t S1, std::size_t S2> consteval auto operator+(const char (&a)[S1], const str_array<S2>& b) {
        return concat<S1, S2>(str_ref<S1>(a), b);
    }
}

template <std::size_t Size> using comptime_str = detail::str_ref<Size>;

}
