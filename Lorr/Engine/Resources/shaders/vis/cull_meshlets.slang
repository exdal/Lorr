import "../lorr";
import vis;

[[vk::binding(0, 0)]]
ConstantBuffer<Scene, ScalarDataLayout> scene;

[[vk::binding(1, 0)]] restrict writeonly
GLSLShaderStorageBuffer<DispatchIndirectCommand, ScalarDataLayout> cull_triangles_cmd;

[[vk::binding(2, 0)]] restrict writeonly
RWStructuredBuffer<u32, ScalarDataLayout> visible_meshlet_instances_indices;

[[vk::binding(3, 0)]] restrict readonly
RWStructuredBuffer<MeshletInstance, ScalarDataLayout> meshlet_instances;

[[vk::binding(4, 0)]] restrict readonly
RWStructuredBuffer<Transform, ScalarDataLayout> transforms;

[[vk::binding(5, 0)]] restrict readonly
RWStructuredBuffer<Model, ScalarDataLayout> models;

#ifndef CULLING_MESHLET_COUNT
#define CULLING_MESHLET_COUNT 64
#endif

bool test_aabb_vs_plane(in float3 center, in float3 extent, in float4 plane) {
    const float3 normal = plane.xyz;
    const float radius = dot(extent, abs(normal));
    return (dot(normal, center) - plane.w) >= -radius;
}

bool test_frustum(in mat4 world_transform, in float3 aabb_center, in float3 aabb_extent) {
    const float3 world_aabb_center = mul(world_transform, float4(aabb_center, 1.0)).xyz;
    const float3 right = world_transform[0].xyz * aabb_extent.x;
    const float3 up = world_transform[1].xyz * aabb_extent.y;
    const float3 forward = -world_transform[2].xyz * aabb_extent.z;
    const float3 world_extent = {
        abs(dot(float3(1.0, 0.0, 0.0), right)) +
        abs(dot(float3(1.0, 0.0, 0.0), up)) +
        abs(dot(float3(1.0, 0.0, 0.0), forward)),

        abs(dot(float3(0.0, 1.0, 0.0), right)) +
        abs(dot(float3(0.0, 1.0, 0.0), up)) +
        abs(dot(float3(0.0, 1.0, 0.0), forward)),

        abs(dot(float3(0.0, 0.0, 1.0), right)) +
        abs(dot(float3(0.0, 0.0, 1.0), up)) +
        abs(dot(float3(0.0, 0.0, 1.0), forward))
    };

    for (uint i = 0; i < 6; i++) {
        if (!test_aabb_vs_plane(world_aabb_center, world_extent, scene.camera.frustum_planes[i])) {
            return false;
        }
    }

    return true;
}

[shader("compute")]
[numthreads(CULLING_MESHLET_COUNT, 1, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    const u32 meshlet_instance_index = thread_id.x;
    if (meshlet_instance_index >= scene.meshlet_instance_count) {
        return;
    }

    const MeshletInstance meshlet_instance = meshlet_instances[meshlet_instance_index];
    const Model model = models[meshlet_instance.mesh_index];
    const u32 meshlet_index = meshlet_instance.meshlet_index;
    const Meshlet meshlet = model.meshlets[meshlet_index];
    const Transform transform = transforms[meshlet_instance.transform_index];
    const MeshletBounds bounds = model.meshlet_bounds[meshlet_index];
    const float3 aabb_min = bounds.aabb_min;
    const float3 aabb_max = bounds.aabb_max;
    const float3 aabb_center = (aabb_min + aabb_max) * 0.5;
    const float3 aabb_extent = aabb_max - aabb_center;

    bool meshlet_passed = true;
    // Frustum culling
    if (scene.cull_flags & CullFlags::MeshletFrustum) {
        meshlet_passed = test_frustum(transform.world, aabb_center, aabb_extent);
    }

    if (meshlet_passed) {
        u32 index = 0;
        InterlockedAdd(cull_triangles_cmd.x, 1, index);
        visible_meshlet_instances_indices[index] = meshlet_instance_index;
    }
}

