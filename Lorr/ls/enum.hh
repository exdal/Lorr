#pragma once

#include <type_traits>

#include <ls/types.hh>

namespace lr {
template<typename T>
struct has_bitmask : std::false_type {};

template<typename T>
std::enable_if_t<has_bitmask<T>::value, T> constexpr operator|(T l, T r) {
    using enum_type_t = std::underlying_type_t<T>;
    return static_cast<T>(static_cast<enum_type_t>(l) | static_cast<enum_type_t>(r));
}

template<typename T>
std::enable_if_t<has_bitmask<T>::value, T &> constexpr operator|=(T &l, T r) {
    return l = l | r;
}

template<typename T>
std::enable_if_t<has_bitmask<T>::value, bool> constexpr operator&(T l, T r) {
    using enum_type_t = std::underlying_type_t<T>;
    return static_cast<bool>(static_cast<enum_type_t>(l) & static_cast<enum_type_t>(r));
}

template<typename T>
std::enable_if_t<has_bitmask<T>::value, T &> constexpr operator&=(T &l, T r) {
    return l = l & r;
}

template<typename T>
std::enable_if_t<has_bitmask<T>::value, T> constexpr operator^(T l, T r) {
    using enum_type_t = std::underlying_type_t<T>;
    return static_cast<T>(static_cast<enum_type_t>(l) ^ static_cast<enum_type_t>(r));
}

template<typename T>
std::enable_if_t<has_bitmask<T>::value, T &> constexpr operator^=(T &l, T r) {
    return l = l ^ r;
}

template<typename T>
std::enable_if_t<has_bitmask<T>::value, T> constexpr operator~(T l) {
    using enum_type_t = std::underlying_type_t<T>;
    return static_cast<T>(~static_cast<enum_type_t>(l));
}
}  // namespace lr
