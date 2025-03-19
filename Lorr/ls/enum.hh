#pragma once

#include <ls/types.hh>
#include <type_traits>
#include <utility>

namespace lr {
template<typename T>
concept BitmaskedEnum = requires(T v) {
    std::is_enum_v<T>;
    enable_bitmask(v);
};

template<BitmaskedEnum T>
constexpr T operator|(T lhs, T rhs) {
    return static_cast<T>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

template<BitmaskedEnum T>
constexpr T &operator|=(T &lhs, T rhs) {
    return lhs = lhs | rhs;
}

template<BitmaskedEnum T>
constexpr bool operator&(T lhs, T rhs) {
    return static_cast<bool>(std::to_underlying(lhs) & std::to_underlying(rhs));
}

template<BitmaskedEnum T>
constexpr T &operator&=(T &lhs, T rhs) {
    return lhs = lhs & rhs;
}

template<BitmaskedEnum T>
constexpr T operator^(T lhs, T rhs) {
    return static_cast<T>(std::to_underlying(lhs) ^ std::to_underlying(rhs));
}

template<BitmaskedEnum T>
constexpr T &operator^=(T &lhs, T rhs) {
    return lhs = lhs ^ rhs;
}
template<BitmaskedEnum T>
constexpr bool operator~(T lhs) {
    return static_cast<T>(~std::to_underlying(lhs));
}
} // namespace lr
