import "../lorr";

[[vk::binding(0, 0)]]
ConstantBuffer<Scene, ScalarDataLayout> scene;

[[vk::binding(1, 0)]] restrict writeonly
GLSLShaderStorageBuffer<DispatchIndirectCommand, ScalarDataLayout> cull_triangles_cmd;

struct VisibleMeshletInstancesIndices {
    u32 indices[];
};
[[vk::binding(2, 0)]] restrict writeonly
GLSLShaderStorageBuffer<VisibleMeshletInstancesIndices, ScalarDataLayout> visible_meshlet_instances_indices;

#ifndef CULLING_MESHLET_COUNT
#define CULLING_MESHLET_COUNT 64
#endif

[shader("compute")]
[numthreads(CULLING_MESHLET_COUNT, 1, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    const u32 meshlet_instance_index = thread_id.x;
    if (meshlet_instance_index >= scene.meshlet_instance_count) {
        return;
    }

    u32 index = 0;
    InterlockedAdd(cull_triangles_cmd.x, 1, index);
    visible_meshlet_instances_indices.indices[index] = meshlet_instance_index;
}

