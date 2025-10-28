import std;
import gpu;
import scene;
import cull;
import vsm;

[[vk::binding(0)]] Texture2DArray<u32> page_hash_table;
[[vk::binding(1)]] Texture2DArray<u32> previous_page_hash_table;
[[vk::binding(2)]] RWTexture2DArray<u32, ImageFormat::R32UI> page_table;

[[shader("compute")]]
[[numthreads(16, 16, 1)]]
func cs_main(
    u32x3 thread_id : SV_DispatchThreadID,
    uniform i32x3 page_table_extent
) -> void {
    if (any(thread_id.xy >= u32x2(page_table_extent.xy))) {
        return;
    }

    let page_address = i32x3(thread_id);
    let current_hash = page_hash_table.Load(i32x4(page_address, 0));
    let previous_hash = previous_page_hash_table.Load(i32x4(page_address, 0));

    if (current_hash != previous_hash) {
        var page = VSMPageMetadata(page_table.Load(page_address));
        if (page.is_backed()) {
            page.reset();
            page.set_invalidated(true);
            page_table.Store(page_address, page.data);
        }
    }
}