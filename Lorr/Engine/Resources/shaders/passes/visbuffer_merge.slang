module visbuffer_merge;

import std;
import gpu;
import scene;
import passes.visbuffer;

#include <fullscreen.slang>

[[vk::binding(0, 0)]]
Image2D<f32> depth_image;

[[vk::binding(1, 0)]]
Image2D<u32> visbuffer_data_image;

[[vk::binding(2, 0)]]
StorageImage2D<u64, ImageFormat::R64UI> visbuffer_image;

struct PushConstants {
    u32x2 extent;
};
[[vk::push_constant]] PushConstants C;

[[shader("compute")]]
[[numthreads(16, 16, 1)]]
func cs_main(u32x2 thread_id : SV_DispatchThreadID) -> void {
    if (any(C.extent < thread_id)) {
        return;
    }

    const f32 depth_texel = depth_image.load(thread_id.xy);
    const u32 vis_texel = visbuffer_data_image.load(thread_id.xy);

    const let vis_data = VisBufferData(vis_texel);
    const let vis = VisBuffer(vis_data, depth_texel);
    visbuffer_image[thread_id.xy] = vis.encode();
}
