import vis;
import "../lorr";

// Resulting buffers
[[vk::binding(0, 0)]] RWStructuredBuffer<DispatchIndirectCommand, ScalarDataLayout> indirect_commands;
[[vk::binding(1, 0)]] RWStructuredBuffer<u32, ScalarDataLayout> visible_meshlet_indices;
[[vk::push_constant]] u32 meshlet_count;

#ifndef CULLING_MESHLET_COUNT
#define CULLING_MESHLET_COUNT 64
#endif

[shader("compute")]
[numthreads(CULLING_MESHLET_COUNT, 1, 1)]
void cs_main(uint3 thread_id : SV_DispatchThreadID) {
    const u32 meshlet_id = thread_id.x;
    if (meshlet_id >= meshlet_count) {
        return;
    }

    u32 index = 0;
    InterlockedAdd(indirect_commands[0].x, 1, index);
    visible_meshlet_indices[index] = meshlet_id;
}

