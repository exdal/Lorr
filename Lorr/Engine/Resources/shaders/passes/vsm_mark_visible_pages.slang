import std;
import gpu;
import scene;
import vsm;

struct ShaderParameters {
    ConstantBuffer<Camera> camera;
    ConstantBuffer<DirectionalLight> directional_light;
    StructuredBuffer<VirtualClipmap> clipmaps;

    Image2D<f32> depth_image;

    StorageImage2D<u32, ImageFormat::R32UI> page_tables;
    RWStructuredBuffer<u32> page_visibility_mask;
};

[[shader("compute")]]
[[numthreads(16, 16, 1)]]
func cs_main(
    u32x2 thread_id : SV_DispatchThreadID,
    uniform ParameterBlock<ShaderParameters> params,
    uniform i32x3 page_table_extent,
    uniform i32x3 depth_extent,
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
    let clipmap_info = clipmap.get_clipmap_info(world_pos, page_table_extent.xy);

    // The algorithm described in the page 3 of the paper:
    // This entire loop is called "waterfall", used to avoid
    // atomic operations per thread, the execution cascades
    // down each iteration to save huge time
    var first_thread_visible = false;
    var done = false;
    while (!done) {
        let subgroup_address = WaveReadLaneFirst(clipmap_info.page_address);
        if (all(subgroup_address == clipmap_info.page_address)) {
            if (WaveIsFirstLane()) {
                first_thread_visible = true;
            }
            done = true;
        }
    }

    if (first_thread_visible) {
        // Set it to visible
        let page_data = __atomic_or(params.page_tables[clipmap_info.page_address], VSM_PAGE_VISIBLE_BIT);
        let page_metadata = VSMPageMetadata(page_data);
        printf("%d", page_metadata.get_physical_address());

        // Did the page just became visible?
        if (!page_metadata.is_visible()) {
            if (page_metadata.is_backed()) {
                // we already have allocated this page, and it was marked visible
                let physical_address = page_metadata.get_physical_address();
                let visibility_mask_index = physical_address / 32;
                let bit_index = physical_address % 32;
                let visibility_bit = 1 << bit_index;
                __atomic_or(params.page_visibility_mask[visibility_mask_index], visibility_bit, MemoryOrder::Relaxed);
            } else {
                // TODO: This is pagefault, handle an allocation request
            }
        }
    }
}
