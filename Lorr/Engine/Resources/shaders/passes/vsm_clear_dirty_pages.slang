import std;
import gpu;
import vsm;

struct ShaderParameters {
    ConstantBuffer<VSMPageAllocator> allocator;
    RWTexture2D<u32, ImageFormat::R32UI> physical_pages;
};

[[shader("compute")]]
[[numthreads(16, 16, 1)]]
func cs_main(
    u32x3 thread_id : SV_DispatchThreadID,
    uniform ParameterBlock<ShaderParameters> params,
    uniform i32 page_size,
    uniform i32 page_table_size
) -> void {
    if (any(thread_id.xy >= u32x2(page_size))) {
        return;
    }

    let physical_page_address = params.allocator.dirty_physical_page_addresses[thread_id.z];
    let physical_page_coords = i32x2(physical_page_address / page_table_size, physical_page_address % page_table_size);
    let physical_page_offset = physical_page_coords * page_size;
    let physical_texel = physical_page_offset + thread_id.xy;
    params.physical_pages.Store(physical_texel, 0);
}
