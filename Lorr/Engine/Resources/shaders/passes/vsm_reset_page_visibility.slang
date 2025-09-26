import std;
import gpu;
import vsm;

struct ShaderParameters {
    StorageImage2D<u32, ImageFormat::R32UI> page_tables;
};

[[shader("compute")]]
[[numthreads(16, 16, 1)]]
func cs_main(
    u32x2 thread_id : SV_DispatchThreadID,
    uniform ParameterBlock<ShaderParameters> params,
    uniform i32x3 page_tables_extent
) -> void {
    if (any(thread_id >= u32x2(page_tables_extent.xy))) {
        return;
    }

    var vsm_meta = VSMPageMetadata(params.page_tables.Load(thread_id));
    vsm_meta.set_visible(false);
    vsm_meta.set_dirty(false);
    params.page_tables.Store(thread_id, vsm_meta.data);
}
