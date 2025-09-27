import std;
import gpu;
import scene;
import vsm;

struct ShaderParameters {
    ConstantBuffer<DirectionalLight> directional_light;
    StructuredBuffer<VirtualClipmap> clipmaps;

    Image2D<f32> clipmap_depth_image;
    Image2D<u32> page_tables;

    StorageImage2D<u32, ImageFormat::R32UI> physical_pages;
};

[[shader("compute")]]
[[numthreads(16, 16, 1)]]
func cs_main(
    u32x2 thread_id : SV_DispatchThreadID,
    uniform ParameterBlock<ShaderParameters> params,
    uniform u32 clipmap_index,
    uniform i32x3 page_table_extent,
    uniform i32x3 physical_page_extent,
    uniform i32x3 depth_extent,
) -> void {
    if (any(thread_id >= u32x2(depth_extent.xy))) {
        return;
    }

    let depth = params.clipmap_depth_image.Load(i32x3(thread_id, 0));
    if (depth == 0.0) {
        return;
    }

    let uv = (f32x2(thread_id.xy) + 0.5) / f32x2(depth_extent.xy);

    let clipmap = params.clipmaps[clipmap_index];
    let page_offset = clipmap.page_offset;
    let page_address = i32x2(fmod(f32x2(i32x2(thread_id) / 128 + page_offset), f32x2(page_table_extent.xy)));
    let page = VSMPageMetadata(params.page_tables.Load(i32x3(page_address, 0)));
    if (page.is_backed() && page.is_dirty()) {
        let pageTexel = i32x2(thread_id.xy) % 128;
        let physical_page_address = page.get_physical_address();
        let atlasWidth = physical_page_extent.x / 128;
        let pageCorner = 128 * i32x2(physical_page_address / atlasWidth, physical_page_address % atlasWidth);
        let depthUint = asuint(depth);
        let physicalTexel = pageCorner + pageTexel;
        __atomic_min(params.physical_pages[physicalTexel], depthUint);
        printf("written %d to %d, %d\n", depthUint, physicalTexel.x, physicalTexel.y);
    }
}
