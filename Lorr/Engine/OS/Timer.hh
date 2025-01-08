#pragma once

#include <chrono>

namespace lr {
struct Timer {
    std::chrono::high_resolution_clock::time_point last_ts = {};

    Timer();
    auto reset() -> void;
    auto elapsed() -> f64;
};
}  // namespace lr
