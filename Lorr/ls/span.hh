#pragma once

#include <span>
#include <vector>

#include "static_vector.hh"

namespace ls {
template<typename T>
concept IsContiguousContainer = requires(T t) {
    { t.begin() } -> std::contiguous_iterator;
    { t.end() } -> std::contiguous_iterator;
};

template<typename T, usize EXTENT = std::dynamic_extent>
struct span : public std::span<T, EXTENT> {
    using this_type = std::span<T, EXTENT>;

    constexpr span() = default;

    template<typename U, usize N>
    constexpr span(const span<U, N> &other)
        : std::span<U>(other.data(), other.size()){};

    constexpr span(this_type::reference v)
        : std::span<T>({ &v, 1 }) {};

    constexpr explicit(EXTENT != std::dynamic_extent) span(T *v, this_type::size_type size)
        : std::span<T>(v, size){};

    constexpr explicit(EXTENT != std::dynamic_extent) span(this_type::iterator v, this_type::size_type size)
        : std::span<T>(v, size){};

    constexpr explicit(EXTENT != std::dynamic_extent) span(this_type::iterator begin_it, this_type::iterator end_it)
        : std::span<T>(begin_it, end_it){};

    template<usize N>
    constexpr span(T (&arr)[N])
        : std::span<T>(arr){};

    template<usize N>
    constexpr span(std::array<T, N> &arr)
        : std::span<T>(arr){};

    template<usize N>
    constexpr span(const std::array<T, N> &arr)
        : std::span<T>(arr){};

    constexpr span(std::vector<T> &v)
        : std::span<T>(v.begin(), v.end()) {};

    constexpr span(const std::vector<T> &v)
        : std::span<T>(v.begin(), v.end()) {};

    template<usize N>
    constexpr span(static_vector<T, N> &arr)
        : std::span<T>(arr.begin(), arr.end()){};

    template<usize N>
    constexpr span(const static_vector<T, N> &arr)
        : std::span<T>(arr.begin(), arr.end()){};
};

template<typename T, usize N>
span(T (&)[N]) -> span<T, N>;

template<typename T, usize N>
span(std::array<T, N> &) -> span<T, N>;

template<typename T, usize N>
span(const std::array<T, N> &) -> span<const T, N>;

template<typename C>
span(C &) -> span<typename C::value_type>;

template<typename C>
span(const C &) -> span<const typename C::value_type>;

}  // namespace ls
