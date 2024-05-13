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

namespace lr {
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

constexpr u32 kib_to_bytes(const u32 x)
{
    return x << 10;
}

constexpr u32 mib_to_bytes(const u32 x)
{
    return x << 20;
}
}  // namespace lr
