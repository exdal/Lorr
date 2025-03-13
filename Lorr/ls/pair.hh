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

template<typename T0, typename T1, typename U0, typename U1>
constexpr bool operator<=>(const pair<T0, T1> &lhs, const pair<U0, U1> &rhs) {
    return std::compare_three_way{}(lhs, rhs);
}

}  // namespace ls
