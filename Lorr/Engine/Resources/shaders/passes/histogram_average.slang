module histogram_average;

import std;
import gpu;
import scene;

#include <passes/histogram.slang>

struct ShaderParameters {
    ConstantBuffer<EyeAdaptation> eye_adaptation;
    StructuredBuffer<u32> histogram_bin_indices;

    RWStructuredBuffer<HistogramLuminance> luminance;
};

groupshared f32 histogram_shared[HISTOGRAM_BIN_COUNT];

func ev100_from_luminance(f32 luminance, f32 ISO_K) -> f32 {
    return log2(luminance * ISO_K);
}

[[shader("compute")]]
[[numthreads(HISTOGRAM_BIN_COUNT, 1, 1)]]
func cs_main(
    u32 gid : SV_GroupIndex,
    uniform ParameterBlock<ShaderParameters> params,
    uniform f32 pixel_count,
    uniform f32 delta_time
) -> void {
    let count_for_this_bin = gid == 0 ? 0.0 : f32(params.histogram_bin_indices[gid]);
    histogram_shared[gid] = count_for_this_bin * f32(gid);
    GroupMemoryBarrierWithGroupSync();

    [[unroll]]
    for (u32 cutoff = (HISTOGRAM_BIN_COUNT >> 1); cutoff > 0; cutoff >>= 1) {
        if (gid < cutoff) {
            histogram_shared[gid] += histogram_shared[gid + cutoff];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (gid == 0) {
        let exposure_range = params.eye_adaptation.max_exposure - params.eye_adaptation.min_exposure;
        let time_coeff = clamp(1.0 - exp(-params.eye_adaptation.adaptation_speed * delta_time), 0.0f, 1.0f);

        let weighted_average_log2 = (histogram_shared[0] / max(pixel_count - count_for_this_bin, 1.0)) - 1.0;
        let desired_luminance =
            exp2(((weighted_average_log2 / (HISTOGRAM_BIN_COUNT - 1)) * exposure_range) + params.eye_adaptation.min_exposure);
        let last_luminance = params.luminance[0].adapted_luminance;
        let adapted_luminance = last_luminance + (desired_luminance - last_luminance) * time_coeff;
        let ev100 = ev100_from_luminance(adapted_luminance, params.eye_adaptation.ISO_K);
        let exposure = 1.0 / (exp2(ev100) * 1.2);
        params.luminance[0] = HistogramLuminance(adapted_luminance, exposure);
    }
}
