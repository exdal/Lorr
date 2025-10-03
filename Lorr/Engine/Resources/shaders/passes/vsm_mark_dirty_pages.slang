import std;
import gpu;
import vsm;

struct ShaderParameters {
    Texture2DArray<u32> page_tables;

    RWStructuredBuffer<DispatchIndirectCommand> clear_cmd;
    RWStructuredBuffer<VSMPageAllocator> allocator;
};

[[shader("compute")]]
[[numthreads(16, 16, 1)]]
func cs_main(
    u32x3 thread_id : SV_DispatchThreadID,
    uniform ParameterBlock<ShaderParameters> params,
    uniform i32x3 page_tables_extent,
    uniform u32 physical_page_image_size,
    uniform u32 page_size
) -> void {
    if (any(thread_id.xy >= u32x2(page_tables_extent.xy))) {
        return;
    }

    var page = VSMPageMetadata(params.page_tables.Load(i32x4(thread_id, 0)));
    if (page.is_backed() && page.is_dirty()) {
        push_dirty_physical_page(params.allocator[0], page.get_physical_coords(physical_page_image_size, page_size));
        __atomic_add(params.clear_cmd[0].z, 1, MemoryOrder::Relaxed);
    }
}
