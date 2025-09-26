import std;
import gpu;
import scene;

[[shader("compute")]]
[[numthreads(1, 1, 1)]]
func cs_main(uniform MeshletInstanceVisibility *visibility, uniform DispatchIndirectCommand *cull_meshlets_cmd) -> void {
    cull_meshlets_cmd.x = (visibility.total_visible_meshlet_instances + (CULLING_MESHLET_COUNT - 1)) / CULLING_MESHLET_COUNT;
}
