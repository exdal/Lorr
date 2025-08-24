import std;
import gpu;
import scene;
import cull;
import debug_drawer;

[[vk::constant_id(0)]] const u32 LATE = 0;
[[vk::binding(0)]] ConstantBuffer<Camera> camera;
[[vk::binding(1)]] StructuredBuffer<Mesh> meshes;
[[vk::binding(2)]] StructuredBuffer<Transform> transforms;
[[vk::binding(3)]] Image2D<f32> hiz_image;
[[vk::binding(4)]] Sampler hiz_sampler;
[[vk::binding(5)]] RWStructuredBuffer<MeshInstance> mesh_instances;
[[vk::binding(6)]] RWStructuredBuffer<MeshletInstance> meshlet_instances;
[[vk::binding(7)]] RWStructuredBuffer<u32> mesh_visibility_mask;
[[vk::binding(8)]] RWStructuredBuffer<u32> early_visible_meshlet_instances_count;
[[vk::binding(9)]] RWStructuredBuffer<u32> late_visible_meshlet_instances_count;
[[vk::binding(10)]] RWStructuredBuffer<DebugDrawer> debug_drawer;

#ifndef CULLING_MESHES_COUNT
    #define CULLING_MESHES_COUNT 64
#endif

[[shader("compute")]]
[[numthreads(CULLING_MESHES_COUNT, 1, 1)]]
func cs_main(
    uint3 thread_id : SV_DispatchThreadID,
    uniform u32 mesh_instances_count,
    uniform CullFlags cull_flags
) -> void {
    let mesh_instance_index = thread_id.x;
    if (mesh_instance_index >= mesh_instances_count) {
        return;
    }

    let was_visible = mesh_visibility_mask[mesh_instance_index] == 1;

    let mesh_instance = &mesh_instances[mesh_instance_index];
    let mesh = meshes[mesh_instance.mesh_index];
    let transform = transforms[mesh_instance.transform_index];
    let mvp = mul(camera.projection_view_mat, transform.world);

    let cull_occlusion = (cull_flags & CullFlags::MeshOcclusion) != 0;

    var visible = true;
    if (LATE == 0 && !was_visible) {
        // During previous frame, if mesh was NOT visible, we don't render it.
        visible = false;
    }

    if (visible && (cull_flags & CullFlags::MeshFrustum)) {
        visible = test_frustum(mvp, mesh.bounds.aabb_center, mesh.bounds.aabb_extent);
    }

    if (LATE == 1 && visible && cull_occlusion) {
        if (let screen_aabb = project_aabb(mvp, camera.near_clip, mesh.bounds.aabb_center, mesh.bounds.aabb_extent)) {
            visible = !test_occlusion(screen_aabb, hiz_image, hiz_sampler, true);

            if (visible && true) {
                let ndc_aabb_max = screen_aabb.max.xy * 2.0 - 1.0;
                let ndc_aabb_min = screen_aabb.min.xy * 2.0 - 1.0;
                var debug_rect = DebugRect();
                debug_rect.offset = f32x3((ndc_aabb_max + ndc_aabb_min) * 0.5, screen_aabb.max.z);
                debug_rect.extent = ndc_aabb_max - ndc_aabb_min;
                debug_rect.color = f32x3(1.0, 0.0, 1.0);
                debug_rect.coord = DebugDrawCoord::NDC;
                debug_draw_rect(debug_drawer[0], debug_rect);
            }
        }
    }

    if (visible && (LATE == 0 || !was_visible)) {
        var lod_index = 0;
#if 1
        // Credits:
        // - https://github.com/Sunset-Flock/Timberdoodle/blob/786f141e261dff4756e7f1a67dd7f7a5e1277956/src/scene/mesh_lod.hpp#L45
        let aabb_center = mul(transform.world, f32x4(mesh.bounds.aabb_center, 1.0)).xyz;
        let aabb_extent_x = length(transform.world[0]) * mesh.bounds.aabb_extent.x;
        let aabb_extent_y = length(transform.world[1]) * mesh.bounds.aabb_extent.y;
        let aabb_extent_z = length(transform.world[2]) * mesh.bounds.aabb_extent.z;
        let aabb_rough_extent = max(max(aabb_extent_x, aabb_extent_y), aabb_extent_z);
        let aabb_rough_camera_distance = max(length(aabb_center - camera.position) - 0.5 * aabb_rough_extent, 0.0);

        // Avoiding the atan here
        let rough_resolution = max(camera.resolution.x, camera.resolution.y);
        let fov90_distance_to_screen_ratio = 2.0f;
        let pixel_size_at_1m = fov90_distance_to_screen_ratio / rough_resolution;
        let aabb_size_at_1m = (aabb_rough_extent / aabb_rough_camera_distance);
        let rough_aabb_pixel_size = aabb_size_at_1m / pixel_size_at_1m;

        for (var i = 1; i < mesh.lod_count; i++) {
            let mesh_lod = mesh.lods[i];
            let rough_pixel_error = rough_aabb_pixel_size * mesh_lod.error;
            if (rough_pixel_error < camera.acceptable_lod_error) {
                lod_index = i;
            } else {
                break;
            }
        }
#endif

        mesh_instance.lod_index = lod_index;
        let mesh_lod = mesh.lods[lod_index];
        var meshlet_instance_offset = 0;
        if (LATE == 0) {
            meshlet_instance_offset = std::atomic_add(early_visible_meshlet_instances_count[0], mesh_lod.meshlet_count, std::memory_order_relaxed);
        } else {
            meshlet_instance_offset = std::atomic_add(late_visible_meshlet_instances_count[0], mesh_lod.meshlet_count, std::memory_order_relaxed);
            meshlet_instance_offset += early_visible_meshlet_instances_count[0];
        }

        for (u32 i = 0; i < mesh_lod.meshlet_count; i++) {
            let offset = meshlet_instance_offset + i;
            var meshlet_instance = MeshletInstance();
            meshlet_instance.mesh_instance_index = mesh_instance_index;
            meshlet_instance.meshlet_index = i;
            meshlet_instances[offset] = meshlet_instance;
        }
    }

    if (LATE == 1) {
        mesh_visibility_mask[mesh_instance_index] = visible ? 1 : 0;
    }
}
