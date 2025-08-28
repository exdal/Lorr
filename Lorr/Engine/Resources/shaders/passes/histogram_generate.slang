module histogram_generate;

import std;
import gpu;
import scene;

#include <passes/histogram.slang>

struct ShaderParameters {
    Image2D<f32x4> src_image;

    ConstantBuffer<Environment> environment;

    RWStructuredBuffer<u32> histogram_bin_indices;
};

groupshared u32 histogram_shared[HISTOGRAM_BIN_COUNT];

func bin_lum(f32 luminance, f32 max_exposure, f32 min_exposure) -> u32 {
    if (luminance < LUMINANCE_EPSILON) {
        return 0;
    }

    let exposure_range_inv = 1.0 / (max_exposure - min_exposure);
    let log_luminance = saturate((log2(luminance) - min_exposure) * exposure_range_inv);
    return u32(log_luminance * f32(HISTOGRAM_BIN_COUNT - 1) + 1.0);
}

[[shader("compute")]]
[[numthreads(HISTOGRAM_THREADS_X, HISTOGRAM_THREADS_Y, 1)]]
func cs_main(
    u32 group_index : SV_GroupIndex,
    u32x3 thread_id : SV_DispatchThreadID,
    uniform ParameterBlock<ShaderParameters> params,
    uniform i32x3 src_extent
) -> void {
    histogram_shared[group_index] = 0;
    GroupMemoryBarrierWithGroupSync();

    if (all(thread_id.xy < src_extent.xy)) {
        const f32x3 color = params.src_image.Load(u32x3(thread_id.xy, 0)).rgb;
        const f32 luminance = std::rec2020_to_xyz(color).y;
        const u32 bin_index = bin_lum(luminance, params.environment.eye_max_exposure, params.environment.eye_min_exposure);
        __atomic_add(histogram_shared[bin_index], 1, MemoryOrder::AcquireRelease);
    }

    GroupMemoryBarrierWithGroupSync();
    __atomic_add(params.histogram_bin_indices[group_index], histogram_shared[group_index], MemoryOrder::Relaxed);
}
