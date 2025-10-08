import std;
import gpu;
import scene;
import cull;
import vsm;

[[vk::binding(0)]] StructuredBuffer<VirtualClipmap> clipmaps;
[[vk::binding(1)]] StructuredBuffer<Mesh> meshes;
[[vk::binding(2)]] StructuredBuffer<MeshInstance> mesh_instances;
[[vk::binding(3)]] StructuredBuffer<u32> dirty_mesh_instance_indices;
[[vk::binding(4)]] StructuredBuffer<Transform> transforms;
[[vk::binding(5)]] RWTexture2DArray<u32, ImageFormat::R32UI> page_tables;

[[shader("compute")]]
[[numthreads(64, 1, 1)]]
func cs_main(
    u32x3 thread_id : SV_DispatchThreadID,
    uniform u32 dirty_mesh_instances_count,
    uniform i32 page_table_size,
    uniform i32 invalidated_page_size,
    uniform i32 invalidated_pages_per_axis
) -> void {
    let dirty_mesh_instance_index = thread_id.x;
    if (dirty_mesh_instance_index >= dirty_mesh_instances_count) {
        return;
    }

    let linear_page_index = thread_id.y;
    let page_x = i32(linear_page_index % invalidated_pages_per_axis);
    let page_y = i32(linear_page_index / invalidated_pages_per_axis);

    let mesh_instance_index = dirty_mesh_instance_indices[dirty_mesh_instance_index];
    let mesh_instance = mesh_instances[mesh_instance_index];
    let mesh = meshes[mesh_instance.mesh_index];
    let transform = transforms[mesh_instance.transform_index];

    let clipmap_index = thread_id.z;
    let clipmap = clipmaps[clipmap_index];
    let mvp = mul(clipmap.projection_view_mat, transform.world);
    if (let screen_aabb = project_aabb(mvp, clipmap.z_near, mesh.bounds.aabb_center, mesh.bounds.aabb_extent)) {
        let texel_min = max(i32x2(floor(screen_aabb.min.xy * page_table_size)), 0);
        let texel_max = min(i32x2(ceil(screen_aabb.max.xy * page_table_size)), page_table_size);
        let texel_bounds_x = i32x2(page_x, page_x + 1) * invalidated_page_size;
        let texel_bounds_y = i32x2(page_y, page_y + 1) * invalidated_page_size;
        let rect_min = max(texel_min, i32x2(texel_bounds_x.x, texel_bounds_y.x));
        let rect_max = min(texel_max, i32x2(texel_bounds_x.y, texel_bounds_y.y));
        for (i32 y = rect_min.y; y < rect_max.y; y++) {
            for (i32 x = rect_min.x; x < rect_max.x; x++) {
                let wrapped_coords = vsm_page_coords_to_wrapped_coords(i32x2(x, y), clipmap.page_offset, page_table_size);
                let page_table_address = i32x3(wrapped_coords, clipmap_index);
                var page = VSMPageMetadata(page_tables.Load(page_table_address));
                if (page.is_backed()) {
                    page.data = 0;
                    page.set_invalidated(true); // DEBUG

                    page_tables.Store(page_table_address, page.data);
                }
            }
        }
    }
}
