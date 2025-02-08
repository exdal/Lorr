import vis;
import "../lorr";

[[vk::binding(0, 0)]] StructuredBuffer<Meshlet, ScalarDataLayout> meshlets;
[[vk::binding(1, 0)]] StructuredBuffer<u32, ScalarDataLayout> visible_meshlet_indices;
// Resulting buffers
[[vk::binding(2, 0)]] RWStructuredBuffer<DrawIndexedIndirectCommand, ScalarDataLayout> indirect_draw_commands;
[[vk::binding(3, 0)]] RWStructuredBuffer<u32, ScalarDataLayout> indirect_indices;
[[vk::push_constant]] u32 meshlet_count;

// Shut up LSP
#ifndef CULLING_TRIANGLE_COUNT
#define CULLING_TRIANGLE_COUNT 64
#endif

groupshared u32 base_index_shared;
groupshared u32 triangles_passed_shared;

[shader("compute")]
[numthreads(CULLING_TRIANGLE_COUNT, 1, 1)]
void cs_main(uint3 group_id : SV_GroupID, uint3 invocation_id : SV_GroupThreadID) {
    const u32 meshlet_id = visible_meshlet_indices[group_id.x];
    const Meshlet meshlet = meshlets[meshlet_id];

    const u32 local_id = invocation_id.x;
    if (local_id == 0) {
        triangles_passed_shared = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    bool primitive_passed = true; // TODO: Properly implement this
    u32 active_triangle_id = 0;
    if (local_id < meshlet.triangle_count) {
        if (primitive_passed) {
            InterlockedAdd(triangles_passed_shared, 1, active_triangle_id);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (local_id == 0) {
        InterlockedAdd(indirect_draw_commands[0].index_count, triangles_passed_shared * 3, base_index_shared);
    }

    GroupMemoryBarrierWithGroupSync();

    if (primitive_passed) {
        u32 triangle_id = local_id * 3;
        u32 index_offset = base_index_shared + active_triangle_id * 3;
        indirect_indices[index_offset + 0] = (meshlet_id << MESHLET_PRIMITIVE_BITS) | ((triangle_id + 0) & MESHLET_PRIMITIVE_MASK);
        indirect_indices[index_offset + 1] = (meshlet_id << MESHLET_PRIMITIVE_BITS) | ((triangle_id + 1) & MESHLET_PRIMITIVE_MASK);
        indirect_indices[index_offset + 2] = (meshlet_id << MESHLET_PRIMITIVE_BITS) | ((triangle_id + 2) & MESHLET_PRIMITIVE_MASK);
    }
}
