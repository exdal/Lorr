#pragma once

namespace ls {
template<typename T0, typename T1>
struct pair {
    T0 n0;
    T1 n1;

    pair() = default;
    constexpr pair(const T0 &n0_, const T1 &n1_)
        : n0(n0_),
          n1(n1_) {}
    constexpr pair(T0 &&n0_, T1 &&n1_)
        : n0(n0_),
          n1(n1_) {}
};

template<typename T0, typename T1>
pair(T0, T1) -> pair<T0, T1>;

}  // namespace ls
