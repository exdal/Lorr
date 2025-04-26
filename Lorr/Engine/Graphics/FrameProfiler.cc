#include "Engine/Graphics/FrameProfiler.hh"

namespace lr {
auto FrameProfiler::measure(this FrameProfiler &self, Device *device, f64 delta_time) -> void {
    ZoneScoped;

    auto &device_runtime = device->get_runtime();

    for (const auto &[pass_name, queries] : device->get_pass_queries()) {
        auto &[start_ts, end_ts] = queries;

        auto start_time = static_cast<f64>(device_runtime.retrieve_timestamp(start_ts).value_or(0));
        auto end_time = static_cast<f64>(device_runtime.retrieve_timestamp(end_ts).value_or(0));
        auto delta = (end_time - start_time) / 1e6;

        auto [stat_it, inserted] = self.pass_stats.try_emplace(pass_name);
        auto &stat = stat_it->second;
        stat.measured_times.add_point(delta);
        stat.moving_avg *= (1.0 - stat.moving_avg_weight) + delta * stat.moving_avg_weight;
    }

    self.accumulated_times.add_point(self.accumulated_time += delta_time);
}

auto FrameProfiler::reset(this FrameProfiler &self) -> void {
    ZoneScoped;

    self.pass_stats.clear();
    self.accumulated_times.erase();
    self.accumulated_time = 0.0f;
}
} // namespace lr
