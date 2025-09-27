import std;
import gpu;
import scene;
import vsm;

struct ShaderParameters {
    ConstantBuffer<VSMPageAllocator> allocator;

    RWStructuredBuffer<u32> page_visibility_mask;
    StorageImage2D<u32, ImageFormat::R32UI> page_tables;
};
ParameterBlock<ShaderParameters> params;

func poll_requests(u32 mask_count, u32 request_index) -> void {
    let request = params.allocator.requests[request_index];
    for (var mask_index = 0; mask_index < mask_count; mask_index++) {
        let mask = params.page_visibility_mask[mask_index];
    }
}

[[shader("compute")]]
[[numthreads(1, 1, 1)]]
func cs_main(
    u32x2 thread_id : SV_DispatchThreadID,
    uniform u32 visible_bit_mask_count,
) -> void {
    for (var i = 0; i < params.allocator.active_request_count; i++) {
        poll_requests(visible_bit_mask_count, i);
    }
}
