import vis;
import "../lorr";

[[vk::binding(0, 0)]] restrict writeonly
GLSLShaderStorageBuffer<DrawIndexedIndirectCommand, ScalarDataLayout> draw_cmd;

struct VisibleMeshletInstancesIndices {
    u32 indices[];
};
[[vk::binding(1, 0)]] restrict readonly
GLSLShaderStorageBuffer<VisibleMeshletInstancesIndices, ScalarDataLayout> visible_meshlet_instances_indices;

struct Meshes {
    Mesh meshes[];
};
[[vk::binding(2, 0)]] restrict readonly
GLSLShaderStorageBuffer<Meshes, ScalarDataLayout> meshes;

struct MeshletInstances {
    MeshletInstance instances[];
};
[[vk::binding(3, 0)]] restrict readonly
GLSLShaderStorageBuffer<MeshletInstances, ScalarDataLayout> meshlet_instances;

struct ReorderedIndices {
    u32 indices[];
};
[[vk::binding(4, 0)]] restrict writeonly
GLSLShaderStorageBuffer<ReorderedIndices, ScalarDataLayout> reordered_indices;

// Shut up LSP
#ifndef CULLING_TRIANGLE_COUNT
#define CULLING_TRIANGLE_COUNT 64
#endif

groupshared u32 base_index_shared;
groupshared u32 triangles_passed_shared;

[shader("compute")]
[numthreads(CULLING_TRIANGLE_COUNT, 1, 1)]
void cs_main(uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID) {
    const u32 local_index = group_thread_id.x;
    const u32 visible_meshlet_index = group_id.x;
    const u32 meshlet_instance_index = visible_meshlet_instances_indices.indices[visible_meshlet_index];
    const MeshletInstance meshlet_instance = meshlet_instances.instances[meshlet_instance_index];
    const Mesh mesh = meshes.meshes[meshlet_instance.mesh_index];
    const u32 meshlet_index = meshlet_instance.meshlet_index;
    const Meshlet meshlet = mesh.meshlets[meshlet_index];

    if (local_index == 0) {
        triangles_passed_shared = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    bool triangle_passed = false;
    u32 active_triangle_index = 0;
    if (local_index < meshlet.triangle_count) {
        triangle_passed = true;
        InterlockedAdd(triangles_passed_shared, 1, active_triangle_index);
    }

    GroupMemoryBarrierWithGroupSync();

    if (local_index == 0) {
        InterlockedAdd(draw_cmd.index_count, meshlet.triangle_count * 3, base_index_shared);
    }

    GroupMemoryBarrierWithGroupSync();

    if (triangle_passed) {
        const u32 triangle_index = local_index * 3;
        const u32 index_offset = base_index_shared + active_triangle_index * 3;
        reordered_indices.indices[index_offset + 0] = (visible_meshlet_index << MESHLET_PRIMITIVE_BITS) | ((triangle_index + 0) & MESHLET_PRIMITIVE_MASK);
        reordered_indices.indices[index_offset + 1] = (visible_meshlet_index << MESHLET_PRIMITIVE_BITS) | ((triangle_index + 1) & MESHLET_PRIMITIVE_MASK);
        reordered_indices.indices[index_offset + 2] = (visible_meshlet_index << MESHLET_PRIMITIVE_BITS) | ((triangle_index + 2) & MESHLET_PRIMITIVE_MASK);
    }
}
