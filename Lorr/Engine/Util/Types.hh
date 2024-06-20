#pragma once

#include <cstddef>
#include <cstdint>

typedef double f64;
typedef float f32;

typedef uint64_t u64;
typedef int64_t i64;

typedef uint32_t u32;
typedef int32_t i32;

typedef uint16_t u16;
typedef int16_t i16;

typedef uint8_t u8;
typedef int8_t i8;

typedef u32 b32;
typedef char c8;
typedef char16_t c16;
typedef char32_t c32;

typedef std::intptr_t uptr;
typedef std::uintptr_t iptr;
typedef std::size_t usize;

// STRING LITERALS
// clang-format off
constexpr u64 operator""_u64(unsigned long long n) { return static_cast<u64>(n); }
constexpr i64 operator""_i64(unsigned long long n) { return static_cast<i64>(n); }
constexpr u32 operator""_u32(unsigned long long n) { return static_cast<u32>(n); }
constexpr i32 operator""_i32(unsigned long long n) { return static_cast<i32>(n); }
constexpr u16 operator""_u16(unsigned long long n) { return static_cast<u16>(n); }
constexpr i16 operator""_i16(unsigned long long n) { return static_cast<i16>(n); }
constexpr u8 operator""_u8(unsigned long long n) { return static_cast<u8>(n); }
constexpr i8 operator""_i8(unsigned long long n) { return static_cast<i8>(n); }

constexpr usize operator""_sz(unsigned long long n) { return static_cast<usize>(n); }

constexpr c8 operator""_c8(unsigned long long n) { return static_cast<c8>(n); }
constexpr c16 operator""_c16(unsigned long long n) { return static_cast<c16>(n); }
constexpr c32 operator""_c32(unsigned long long n) { return static_cast<c32>(n); }
// clang-format on

namespace lr {
// MINMAX
template<typename T>
const T &min(const T &a, const T &b)
{
    return (b < a) ? b : a;
}

template<typename T>
const T &max(const T &a, const T &b)
{
    return (a < b) ? b : a;
}

template<typename T>
union range_t {
    using this_type = range_t<T>;

    this_type &clamp(T val)
    {
        this->min = lr::min(min, val);
        this->max = lr::min(max, val);

        return *this;
    }

    T length() { return max - min; }

    T v[2] = {};
    struct {
        T min;
        T max;
    };
};

using u8range = range_t<u8>;
using i8range = range_t<i8>;
using u16range = range_t<u16>;
using i16range = range_t<i16>;
using u32range = range_t<u32>;
using i32range = range_t<i32>;
using u64range = range_t<u64>;
using i64range = range_t<i64>;

using f32range = range_t<f32>;
using f64range = range_t<f64>;

template<typename T>
T align_up(T size, u64 alignment)
{
    return T((u64(size) + (alignment - 1)) & ~(alignment - 1));
}

template<typename T>
T align_down(T size, u64 alignment)
{
    return T(u64(size) & ~(alignment - 1));
}

template<typename T>
constexpr T kib_to_bytes(const T x)
{
    return x << static_cast<T>(10);
}

template<typename T>
constexpr T mib_to_bytes(const T x)
{
    return x << static_cast<T>(20);
}

#define LR_HANDLE(name, type) enum class name : type { Invalid = std::numeric_limits<type>::max() }

}  // namespace lr
