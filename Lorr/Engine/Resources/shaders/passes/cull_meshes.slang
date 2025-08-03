import std;
import gpu;
import scene;
import cull;
import debug_drawer;

struct ShaderParameters {
    ConstantBuffer<Camera> camera;
    StructuredBuffer<MeshInstance> mesh_instances;
    StructuredBuffer<Mesh> meshes;
    StructuredBuffer<Transform> transforms;

    RWStructuredBuffer<DispatchIndirectCommand> cull_meshlets_cmd;
    RWStructuredBuffer<u32> visible_mesh_instances_indices;
    RWStructuredBuffer<DebugDrawer> debug_drawer;
};

#ifndef CULLING_MESHES_COUNT
    #define CULLING_MESHES_COUNT 64
#endif

#ifndef CULLING_MESHLET_COUNT
    #define CULLING_MESHLET_COUNT 64
#endif

[[shader("compute")]]
[[numthreads(CULLING_MESHES_COUNT, 1, 1)]]
func cs_main(
    uint3 thread_id : SV_DispatchThreadID,
    uniform ParameterBlock<ShaderParameters> params,
    uniform u32 mesh_instances_count,
    uniform CullFlags cull_flags
) -> void {
    let mesh_instance_index = thread_id.x;
    if (mesh_instance_index >= mesh_instances_count) {
        return;
    }

    let mesh_instance = params.mesh_instances[mesh_instance_index];
    let mesh = params.meshes[mesh_instance.mesh_index];
    let mesh_lod = mesh.lods[mesh_instance.lod_index];
    let transform = params.transforms[mesh_instance.transform_index];

    let aabb_min = mesh.bounds.aabb_min;
    let aabb_max = mesh.bounds.aabb_max;
    let aabb_extent = aabb_max - aabb_min;
    let aabb_center = (aabb_min + aabb_max) * 0.5;

    var visible = true;
    if (visible && (cull_flags & CullFlags::MeshletFrustum)) {
        let cur_mvp = mul(params.camera.projection_view_mat, transform.world);
        visible = test_frustum(cur_mvp, aabb_center, aabb_extent);

        var debug_aabb = DebugAABB();
        debug_aabb.position = mul(transform.world, f32x4(aabb_center, 1.0)).xyz;
        debug_aabb.size = mul(transform.world, f32x4(aabb_extent, 0.0)).xyz;
        debug_aabb.color = f32x3(0.0, 1.0, 0.0);
        debug_aabb.coord = DebugDrawCoord::World;
        debug_draw_aabb(params.debug_drawer[0], debug_aabb);
    }

    if (visible) {
        let workgroup_count = (mesh_lod.meshlet_count + (CULLING_MESHLET_COUNT - 1)) / CULLING_MESHLET_COUNT;
        let index = std::atomic_add(params.cull_meshlets_cmd[0].x, workgroup_count, std::memory_order_relaxed);
        // params.visible_mesh_instances_indices[index] = mesh_instance_index;
    }
}
