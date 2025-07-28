module editor_mousepick;

import std;
import gpu;
import scene;

import passes.visbuffer;

[[vk::binding(0, 0)]]
Image2D<u32> visbuffer_data;

[[vk::binding(1, 0)]]
StructuredBuffer<MeshletInstance, ScalarDataLayout> meshlet_instances;

[[vk::binding(2, 0)]]
StructuredBuffer<MeshInstance, ScalarDataLayout> mesh_instances;

struct PushConstants {
    u32 *dst;
    u32x2 texel;
};

[[vk::push_constant]] PushConstants C;

[[shader("compute")]]
[[numthreads(1, 1, 1)]]
func cs_main() -> void {
    const u32 texel = visbuffer_data.load(C.texel);
    if (texel == ~0u) {
        *C.dst = ~0u;
        return;
    }

    let vis = VisBufferData(texel);
    let meshlet_instance = meshlet_instances[vis.meshlet_instance_index];
    let mesh_instance = mesh_instances[meshlet_instance.mesh_instance_index];
    *C.dst = mesh_instance.transform_index;
}
