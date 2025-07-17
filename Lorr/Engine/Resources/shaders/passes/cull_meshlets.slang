import std;
import gpu;
import scene;
import cull;
import debug_drawer;

struct ShaderParameters {
    RWStructuredBuffer<DispatchIndirectCommand> cull_triangles_cmd;
    ConstantBuffer<Camera> camera;
    RWStructuredBuffer<u32> visible_meshlet_instances_indices;
    StructuredBuffer<MeshletInstance> meshlet_instances;
    StructuredBuffer<Mesh> meshes;
    StructuredBuffer<Transform> transforms;
    Image2D<f32> hiz_image;
    Sampler hiz_sampler;
    RWStructuredBuffer<DebugDrawer> debug_drawer;
};

func test_frustum(in f32x4x4 mvp, in f32x3 aabb_center, in f32x3 aabb_extent) -> bool {
    f32x4 planes[] = {
        normalize_plane(mvp[3] + mvp[0]),
        normalize_plane(mvp[3] - mvp[0]),
        normalize_plane(mvp[3] + mvp[1]),
        normalize_plane(mvp[3] - mvp[1]),
        normalize_plane(mvp[2])
    };

    let aabb_half_extent = aabb_extent * 0.5;
    [[unroll]]
    for (uint i = 0; i < planes.getCount(); i++) {
        let flip = asuint(planes[i].xyz) & 0x80000000;
        let sign_flipped = asfloat(asuint(aabb_half_extent) ^ flip);
        if (dot(aabb_center + sign_flipped, planes[i].xyz) <= -planes[i].w) {
            return false;
        }
    }

    return true;
}

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
    let mesh = params.meshes[meshlet_instance.mesh_index];
    let meshlet_index = meshlet_instance.meshlet_index;
    let meshlet = mesh.meshlets[meshlet_index];
    let transform = params.transforms[meshlet_instance.transform_index];
    let bounds = mesh.meshlet_bounds[meshlet_index];

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
            visible = !test_occlusion(screen_aabb, params.hiz_image, params.hiz_sampler, params.camera.resolution);
            if (visible) {
                let ndc_aabb_max = screen_aabb.max.xy * 2.0 - 1.0;
                let ndc_aabb_min = screen_aabb.min.xy * 2.0 - 1.0;
                var debug_rect = DebugRect();
                debug_rect.offset = f32x3((ndc_aabb_max + ndc_aabb_min) * 0.5, screen_aabb.max.z);
                debug_rect.extent = ndc_aabb_max - ndc_aabb_min;
                debug_rect.color = f32x3(1.0, 0.0, 0.0);
                debug_rect.coord = DebugDrawCoord::NDC;
                debug_draw_rect(params.debug_drawer[0], debug_rect);

                var debug_aabb = DebugAABB();
                debug_aabb.position = mul(transform.world, f32x4(aabb_center, 1.0)).xyz;
                debug_aabb.size = mul(transform.world, f32x4(aabb_extent, 0.0)).xyz;
                debug_aabb.color = f32x3(0.0, 1.0, 0.0);
                debug_aabb.coord = DebugDrawCoord::World;
                debug_draw_aabb(params.debug_drawer[0], debug_aabb);
            }
        }
    }

    if (visible) {
        let index = std::atomic_add(params.cull_triangles_cmd[0].x, 1, std::memory_order_relaxed);
        params.visible_meshlet_instances_indices[index] = meshlet_instance_index;
    }
}