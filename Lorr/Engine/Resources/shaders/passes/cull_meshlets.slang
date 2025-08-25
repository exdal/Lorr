import std;
import gpu;
import scene;
import cull;
import debug_drawer;

#include <assert.slang>

[[vk::binding(0)]] ConstantBuffer<Camera> camera;
[[vk::binding(1)]] StructuredBuffer<MeshletInstance> meshlet_instances;
[[vk::binding(2)]] StructuredBuffer<MeshInstance> mesh_instances;
[[vk::binding(3)]] StructuredBuffer<Mesh> meshes;
[[vk::binding(4)]] StructuredBuffer<Transform> transforms;
[[vk::binding(5)]] Image2D<f32> hiz_image;
[[vk::binding(6)]] Sampler hiz_sampler;
[[vk::binding(7)]] StructuredBuffer<u32> visible_meshlet_instances_count;
[[vk::binding(8)]] RWStructuredBuffer<u32> visible_meshlet_instances_indices;
[[vk::binding(9)]] RWStructuredBuffer<DispatchIndirectCommand> cull_triangles_cmd;
[[vk::binding(10)]] RWStructuredBuffer<DebugDrawer> debug_drawer;

#ifndef CULLING_MESHLET_COUNT
    #define CULLING_MESHLET_COUNT 64
#endif

[[shader("compute")]]
[[numthreads(CULLING_MESHLET_COUNT, 1, 1)]]
func cs_main(
    uint3 thread_id : SV_DispatchThreadID,
    uniform CullFlags cull_flags
) -> void {
    let meshlet_instance_count = visible_meshlet_instances_count[0];
    let meshlet_instance_index = thread_id.x;
    if (meshlet_instance_index >= meshlet_instance_count) {
        return;
    }

    let meshlet_instance = meshlet_instances[meshlet_instance_index];
    let mesh_instance = mesh_instances[meshlet_instance.mesh_instance_index];
    let mesh = meshes[mesh_instance.mesh_index];
    let transform = transforms[mesh_instance.transform_index];
    let mesh_lod = mesh.lods[mesh_instance.lod_index];
    let bounds = mesh_lod.meshlet_bounds[meshlet_instance.meshlet_index];

    let cull_frustum = (cull_flags & CullFlags::MeshletFrustum) != 0;
    let cull_occlusion = (cull_flags & CullFlags::MeshletOcclusion) != 0;

    var visible = true;
    if (visible) {
        let mvp = mul(camera.projection_view_mat, transform.world);
        visible = test_frustum(mvp, bounds.aabb_center, bounds.aabb_extent);
    }

    if (visible && cull_occlusion) {
        let mvp_prev = mul(camera.prev_projection_view_mat, transform.world);
        if (let screen_aabb = project_aabb(mvp_prev, camera.near_clip, bounds.aabb_center, bounds.aabb_extent)) {
            visible = !test_occlusion(screen_aabb, hiz_image, hiz_sampler, false);

            if (visible && true) {
                let ndc_aabb_max = screen_aabb.max.xy * 2.0 - 1.0;
                let ndc_aabb_min = screen_aabb.min.xy * 2.0 - 1.0;
                var debug_rect = DebugRect();
                debug_rect.offset = f32x3((ndc_aabb_max + ndc_aabb_min) * 0.5, screen_aabb.max.z);
                debug_rect.extent = ndc_aabb_max - ndc_aabb_min;
                debug_rect.color = f32x3(1.0, 0.0, 0.0);
                debug_rect.coord = DebugDrawCoord::NDC;
                debug_draw_rect(debug_drawer[0], debug_rect);
            }
        }
    }

    if (visible) {
        let index = std::atomic_add(cull_triangles_cmd[0].x, 1, std::memory_order_relaxed);
        visible_meshlet_instances_indices[index] = meshlet_instance_index;
    }
}
