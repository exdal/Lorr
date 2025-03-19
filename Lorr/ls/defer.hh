#pragma once

#include <algorithm>

#include <ls/core.hh>

namespace ls {
template<typename Fn>
struct defer {
    Fn func;

    defer(Fn func_): func(std::move(func_)) {}

    ~defer() {
        func();
    }
};

#define LS_DEFER(...) ::ls::defer LS_UNIQUE_VAR() = [__VA_ARGS__]

} // namespace ls
