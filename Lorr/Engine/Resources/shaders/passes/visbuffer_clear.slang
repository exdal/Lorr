module visbuffer_clear;

import std;
import gpu;
import passes.visbuffer;

[[vk::binding(0, 0)]]
StorageImage2D<u64, ImageFormat::R64UI> visbuffer_image;

[[vk::binding(1, 0)]]
StorageImage2D<u32, ImageFormat::R32UI> visbuffer_data_image;

[[vk::binding(2, 0)]]
StorageImage2D<u32, ImageFormat::R32UI> overdraw_image;

[[shader("compute")]]
[[numthreads(1, 1, 1)]]
void cs_main(u32x3 thread_id : SV_DispatchThreadID) {
    const let vis_data = VisBufferData(~0u, ~0u);
    const let vis = VisBuffer(vis_data, 1.0);

    visbuffer_image[thread_id.xy] = vis.encode();
    visbuffer_data_image[thread_id.xy] = vis_data.encode();
    overdraw_image[thread_id.xy] = 0u;
}
