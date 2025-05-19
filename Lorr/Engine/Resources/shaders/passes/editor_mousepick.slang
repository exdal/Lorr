module editor_mousepick;

import std;
import gpu;
import scene;

import passes.visbuffer;

[[vk::binding(0, 0)]]
Image2D<u32> visbuffer_data;

[[vk::binding(1, 0)]]
StructuredBuffer<u32, ScalarDataLayout> visible_meshlet_instances_indices;

[[vk::binding(2, 0)]]
StructuredBuffer<MeshletInstance, ScalarDataLayout> meshlet_instances;

struct PushConstants {
    u32 *dst;
    u32x2 texel;
};

[[vk::push_constant]] PushConstants C;

[[shader("compute")]] [[numthreads(1, 1, 1)]]
void cs_main() {
    const u32 texel = visbuffer_data.load(C.texel);
    if (texel == ~0u) {
        *C.dst = ~0u;
        return;
    }

    const let vis = VisBufferData(texel);
    const u32 meshlet_instance_index = visible_meshlet_instances_indices[vis.meshlet_instance_index];
    const MeshletInstance meshlet_instance = meshlet_instances[meshlet_instance_index];
    *C.dst = meshlet_instance.transform_index;
}
