#pragma once

#include "Engine/Graphics/VulkanDevice.hh"

#include "Engine/Memory/ScrollingBuffer.hh"

namespace lr {
struct PassStat {
    ScrollingBuffer<f64, 10000> measured_times = {};
};

struct FrameProfiler {
    ankerl::unordered_dense::map<vuk::Name, PassStat> pass_stats = {};
    ScrollingBuffer<f64, 10000> accumulated_times = {};
    f64 accumulated_time = 0.0;

    auto measure(this FrameProfiler &, Device *device, f64 delta_time) -> void;
    auto reset(this FrameProfiler &) -> void;
};
}
