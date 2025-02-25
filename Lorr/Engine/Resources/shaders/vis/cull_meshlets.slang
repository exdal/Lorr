import "../lorr";

[[vk::binding(0, 0)]] restrict writeonly
GLSLShaderStorageBuffer<DispatchIndirectCommand, ScalarDataLayout> indirect_command;

struct VisibleMeshletInstancesIndices {
    u32 indices[];
};
[[vk::binding(1, 0)]] restrict writeonly
GLSLShaderStorageBuffer<VisibleMeshletInstancesIndices, ScalarDataLayout> visible_meshlet_instances_indices;

[[vk::push_constant]] u32 meshlet_count;

#ifndef CULLING_MESHLET_COUNT
#define CULLING_MESHLET_COUNT 64
#endif

[shader("compute")]
[numthreads(CULLING_MESHLET_COUNT, 1, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    const u32 meshlet_index = thread_id.x;
    if (meshlet_index >= meshlet_count) {
        return;
    }

    u32 index = 0;
    InterlockedAdd(indirect_command.x, 1, index);
    visible_meshlet_instances_indices.indices[index] = meshlet_index;
}

