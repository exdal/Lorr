import std;
import gpu;
import scene;
import vsm;

struct ShaderParameters {
    ConstantBuffer<Camera> camera;
    ConstantBuffer<DirectionalLight> directional_light;
    StructuredBuffer<VirtualClipmap> clipmaps;

    Image2D<f32> depth_image;

    RWTexture2DArray<u32, ImageFormat::R32UI> page_tables;
    RWStructuredBuffer<u32> page_visibility_mask;
    RWStructuredBuffer<VSMPageAllocator> allocator;
};

[[shader("compute")]]
[[numthreads(16, 16, 1)]]
func cs_main(
    u32x2 thread_id : SV_DispatchThreadID,
    uniform ParameterBlock<ShaderParameters> params,
    uniform i32x3 depth_extent,
    uniform i32 page_table_size,
) -> void {
    if (any(thread_id >= u32x2(depth_extent.xy))) {
        return;
    }

    let depth = params.depth_image.Load(i32x3(thread_id, 0));
    if (depth == 0.0) {
        return;
    }

    let uv = (f32x2(thread_id.xy) + 0.5) / f32x2(depth_extent.xy);
    let world_pos = params.camera.unproject_uv(uv, depth);

    let clipmap_index = params.directional_light.get_clipmap_index(params.camera, uv, depth);
    let clipmap = params.clipmaps[clipmap_index];
    let clipmap_info = clipmap.get_clipmap_info(world_pos);
    let wrapped_coords = vsm_clip_info_to_wrapped_coords(clipmap_info.clip_uv, clipmap.page_offset, page_table_size);
    let page_table_address = i32x3(wrapped_coords, clipmap_index);

    // The algorithm described in the page 3 of the paper:
    // This entire loop is called "waterfall", used to avoid
    // atomic operations per thread, the execution cascades
    // down each iteration to save huge time
    var first_thread_visible = all(wrapped_coords >= 0);
    var done = false;
    while (!done) {
        let subgroup_address = WaveReadLaneFirst(page_table_address);
        if (all(subgroup_address == page_table_address)) {
            if (WaveIsFirstLane()) {
                first_thread_visible = true;
            }
            done = true;
        }
    }

    if (first_thread_visible) {
        // Mark it as visible
        let prev_page_data = __atomic_or(params.page_tables[page_table_address], u32(VSMPageState.Visible), MemoryOrder::Relaxed);
        let prev_page = VSMPageMetadata(prev_page_data);

        // Did the page just became visible?
        if (!prev_page.is_visible()) {
            if (prev_page.is_backed()) {
                // we already have allocated this page, and it was marked visible
                // keep track of visible and backed pages so the allocator wont overwrite it
                let physical_address = prev_page.get_physical_address();
                let visibility_mask_index = physical_address / 32;
                let bit_index = physical_address % 32;
                let visibility_bit = 1 << bit_index;
                __atomic_or(params.page_visibility_mask[visibility_mask_index], visibility_bit, MemoryOrder::Relaxed);
            } else {
                let alloc_request = VSMAllocRequest(page_table_address);
                push_page_alloc_request(params.allocator[0], alloc_request);
            }
        }
    }
}
