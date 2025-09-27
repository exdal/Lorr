import std;
import gpu;
import scene;
import cull;
import debug_drawer;

#ifndef CULLING_MESH_COUNT
#define CULLING_MESH_COUNT 64
#endif

[[vk::binding(0)]] ConstantBuffer<Camera> camera;
[[vk::binding(1)]] StructuredBuffer<Mesh> meshes;
[[vk::binding(2)]] RWStructuredBuffer<MeshInstance> mesh_instances;
[[vk::binding(3)]] RWStructuredBuffer<MeshletInstance> meshlet_instances;
[[vk::binding(4)]] RWStructuredBuffer<MeshletInstanceVisibility> visibility;
[[vk::binding(5)]] StructuredBuffer<Transform> transforms;

[[shader("compute")]]
[[numthreads(CULLING_MESH_COUNT, 1, 1)]]
func cs_main(
    uint3 thread_id : SV_DispatchThreadID,
    uniform u32 mesh_instances_count,
    uniform CullFlags cull_flags,
    // uniform DebugDrawer *debug_drawer,
) -> void {
    let mesh_instance_index = thread_id.x;
    if (mesh_instance_index >= mesh_instances_count) {
        return;
    }

    let mesh_instance = mesh_instances[mesh_instance_index];
    let mesh = meshes[mesh_instance.mesh_index];
    let transform = transforms[mesh_instance.transform_index];
    let mvp = mul(camera.projection_view_mat, transform.world);

    let cull_frustum = (cull_flags & CullFlags::MeshFrustum) != 0;
    if (cull_frustum && !test_frustum(mvp, mesh.bounds.aabb_center, mesh.bounds.aabb_extent)) {
        return;
    }

    let aabb_center = mul(transform.world, f32x4(mesh.bounds.aabb_center, 1.0)).xyz;
    let aabb_extent = mul(transform.world, f32x4(mesh.bounds.aabb_extent, 0.0)).xyz;
    var lod_index = 0;
#if 1
    // Credits:
    // - https://github.com/Sunset-Flock/Timberdoodle/blob/786f141e261dff4756e7f1a67dd7f7a5e1277956/src/scene/mesh_lod.hpp#L45
    let aabb_rough_extent = max(aabb_extent.x, max(aabb_extent.y, aabb_extent.z));
    let aabb_rough_camera_distance = max(length(aabb_center - camera.position) - 0.5 * aabb_rough_extent, 0.0);

    let fov90_distance_to_screen_ratio = 2.0f;
    let pixel_size_at_1m = fov90_distance_to_screen_ratio / max(camera.resolution.x, camera.resolution.y);
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

#if 0
    if (debug_drawer) {
        var debug_aabb = DebugAABB();
        debug_aabb.position = aabb_center;
        debug_aabb.size = aabb_extent;
        debug_aabb.color = f32x3(0.0, 0.0, 1.0);
        debug_aabb.coord = DebugDrawCoord::World;
        debug_draw_aabb(debug_drawer[0], debug_aabb);
    }
#endif

    mesh_instances[mesh_instance_index].lod_index = lod_index;
    let mesh_lod = mesh.lods[lod_index];
    let meshlet_count = mesh_lod.meshlet_count;
    var base_meshlet_instance_offset = __atomic_add(
        visibility[0].total_visible_meshlet_instances, meshlet_count, MemoryOrder::Relaxed);
    for (u32 i = 0; i < meshlet_count; i++) {
        let offset = base_meshlet_instance_offset + i;
        meshlet_instances[offset].mesh_instance_index = mesh_instance_index;
        meshlet_instances[offset].meshlet_index = i;
    }
}
