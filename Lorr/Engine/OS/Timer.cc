#include "Engine/OS/Timer.hh"

namespace lr {
Timer::Timer() {
    reset();
}

auto Timer::reset() -> void {
    last_ts = std::chrono::high_resolution_clock::now();
}

auto Timer::elapsed() -> f64 {
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<f64> delta = now - last_ts;
    return delta.count();
}
}  // namespace lr
