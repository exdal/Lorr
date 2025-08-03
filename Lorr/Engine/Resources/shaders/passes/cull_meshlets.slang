import std;
import gpu;
import scene;
import cull;
import debug_drawer;

#include <assert.slang>

struct ShaderParameters {
    ConstantBuffer<Camera> camera;
    StructuredBuffer<MeshletInstance> meshlet_instances;
    StructuredBuffer<MeshInstance> mesh_instances;
    StructuredBuffer<Mesh> meshes;
    StructuredBuffer<Transform> transforms;
    Image2D<f32> hiz_image;
    Sampler hiz_sampler;

    RWStructuredBuffer<DispatchIndirectCommand> cull_triangles_cmd;
    RWStructuredBuffer<u32> visible_meshlet_instances_indices;
    RWStructuredBuffer<DebugDrawer> debug_drawer;
};

#ifndef CULLING_MESHLET_COUNT
    #define CULLING_MESHLET_COUNT 64
#endif

[[shader("compute")]]
[[numthreads(CULLING_MESHLET_COUNT, 1, 1)]]
func cs_main(
    uint3 thread_id : SV_DispatchThreadID,
    uniform ParameterBlock<ShaderParameters> params,
    uniform u32 meshlet_instance_count,
    uniform CullFlags cull_flags
) -> void {
    let meshlet_instance_index = thread_id.x;
    if (meshlet_instance_index >= meshlet_instance_count) {
        return;
    }

    let meshlet_instance = params.meshlet_instances[meshlet_instance_index];
    let mesh_instance = params.mesh_instances[meshlet_instance.mesh_instance_index];
    let mesh = params.meshes[mesh_instance.mesh_index];
    let transform = params.transforms[mesh_instance.transform_index];
    let mesh_lod = mesh.lods[mesh_instance.lod_index];
    let bounds = mesh_lod.meshlet_bounds[meshlet_instance.meshlet_index];

    let aabb_min = bounds.aabb_min;
    let aabb_max = bounds.aabb_max;
    let aabb_extent = aabb_max - aabb_min;
    let aabb_center = (aabb_min + aabb_max) * 0.5;

    var visible = true;
    if (visible && (cull_flags & CullFlags::MeshletFrustum)) {
        let cur_mvp = mul(params.camera.projection_view_mat, transform.world);
        visible = test_frustum(cur_mvp, aabb_center, aabb_extent);
    }

    if (visible && (cull_flags & CullFlags::Occlusion)) {
        let prev_mvp = mul(params.camera.frustum_projection_view_mat, transform.world);
        if (let screen_aabb = project_aabb(prev_mvp, params.camera.near_clip, aabb_min, aabb_extent)) {
            visible = !test_occlusion(screen_aabb, params.hiz_image, params.hiz_sampler);
            if (visible) {
                let ndc_aabb_max = screen_aabb.max.xy * 2.0 - 1.0;
                let ndc_aabb_min = screen_aabb.min.xy * 2.0 - 1.0;
                var debug_rect = DebugRect();
                debug_rect.offset = f32x3((ndc_aabb_max + ndc_aabb_min) * 0.5, screen_aabb.max.z);
                debug_rect.extent = ndc_aabb_max - ndc_aabb_min;
                debug_rect.color = f32x3(1.0, 0.0, 0.0);
                debug_rect.coord = DebugDrawCoord::NDC;
                debug_draw_rect(params.debug_drawer[0], debug_rect);
            }
        }
    }

    if (visible) {
        let index = std::atomic_add(params.cull_triangles_cmd[0].x, 1, std::memory_order_relaxed);
        assert(index < meshlet_instance_count);
        params.visible_meshlet_instances_indices[index] = meshlet_instance_index;
    }
}
