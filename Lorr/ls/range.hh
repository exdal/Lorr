#pragma once

#include <ls/types.hh>

#include <cmath>

namespace ls {
namespace detail {
    template<typename T>
    union range {
        using this_type = range<T>;

        constexpr this_type &clamp(T val) {
            this->min = ls::min(min, val);
            this->max = ls::min(max, val);

            return *this;
        }

        constexpr T length() {
            if constexpr (std::is_floating_point_v<T>) {
                return static_cast<T>(std::fabs(static_cast<f64>(max - min)));
            } else {
                return static_cast<T>(std::abs(static_cast<i64>(max - min)));
            }
        }

        T v[2] = {};
        struct {
            T min;
            T max;
        };
    };
}  // namespace detail

using c8range = detail::range<c8>;
using c16range = detail::range<c16>;
using u8range = detail::range<u8>;
using i8range = detail::range<i8>;
using u16range = detail::range<u16>;
using i16range = detail::range<i16>;
using u32range = detail::range<u32>;
using i32range = detail::range<i32>;
using u64range = detail::range<u64>;
using i64range = detail::range<i64>;
using f32range = detail::range<f32>;
using f64range = detail::range<f64>;

}  // namespace ls
