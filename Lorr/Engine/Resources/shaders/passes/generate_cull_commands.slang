import std;
import gpu;
import scene;

[[vk::binding(0)]] ConstantBuffer<MeshletInstanceVisibility> visibility;
[[vk::binding(1)]] RWStructuredBuffer<DispatchIndirectCommand> cull_meshlets_cmd;

[[shader("compute")]]
[[numthreads(1, 1, 1)]]
func cs_main() -> void {
    cull_meshlets_cmd[0].x = (visibility.total_visible_meshlet_instances + (CULLING_MESHLET_COUNT - 1)) / CULLING_MESHLET_COUNT;
}
