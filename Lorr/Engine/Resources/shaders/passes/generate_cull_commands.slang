import std;
import gpu;

[[vk::constant_id(0)]] const u32 LATE = 0;
[[vk::binding(0)]] StructuredBuffer<u32> early_visible_meshlet_instances_count;
[[vk::binding(1)]] StructuredBuffer<u32> late_visible_meshlet_instances_count;
[[vk::binding(2)]] RWStructuredBuffer<DispatchIndirectCommand> cull_meshlets_cmd;

[[shader("compute")]]
[[numthreads(1, 1, 1)]]
func cs_main() -> void {
    if (LATE == 0) {
        cull_meshlets_cmd[0].x = (early_visible_meshlet_instances_count[0] + (CULLING_MESHLET_COUNT - 1)) / CULLING_MESHLET_COUNT;
    } else {
        cull_meshlets_cmd[0].x = (late_visible_meshlet_instances_count[0] + (CULLING_MESHLET_COUNT - 1)) / CULLING_MESHLET_COUNT;
    }
}
