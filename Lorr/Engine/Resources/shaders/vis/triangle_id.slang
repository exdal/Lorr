import vis;
import "../lorr";

[[vk::binding(0, 0)]] ConstantBuffer<Scene, ScalarDataLayout> scene;

struct Transforms {
    Transform transforms[];
};
[[vk::binding(1, 0)]] restrict readonly
GLSLShaderStorageBuffer<Transforms, ScalarDataLayout> transforms;

struct Models {
    Model models[];
};
[[vk::binding(2, 0)]] restrict readonly
GLSLShaderStorageBuffer<Models, ScalarDataLayout> models;

struct MeshletInstances {
    MeshletInstance instances[];
};
[[vk::binding(3, 0)]] restrict readonly
GLSLShaderStorageBuffer<MeshletInstances, ScalarDataLayout> meshlet_instances;

struct VertexOutput {
    float4 position  : SV_Position;
    u32 meshlet_id   : MESHLET_ID;
};

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    const u32 meshlet_instance_index = (vertex_id >> MESHLET_PRIMITIVE_BITS) & MESHLET_ID_MASK;
    const u32 primitive_index = vertex_id & MESHLET_PRIMITIVE_MASK;

    const MeshletInstance meshlet_instance = meshlet_instances.instances[meshlet_instance_index];
    const Model model = models.models[meshlet_instance.model_index];
    const Meshlet meshlet = model.meshlets[meshlet_instance.meshlet_index];
    const uint vertex_offset = meshlet.vertex_offset;
    const uint index_offset = meshlet.index_offset;
    const uint triangle_offset = meshlet.triangle_offset;

    const u32 primitive = u32(model.local_triangle_indices[triangle_offset + primitive_index]);
    const u32 index = model.indices[index_offset + primitive];
    const float3 vertex_pos = model.vertex_positions[vertex_offset + index];
    const Transform transform = transforms.transforms[meshlet_instance.transform_index];

    VertexOutput output;
    output.position = mul(transform.local, float4(vertex_pos, 1.0));
    output.position = mul(scene.camera.projection_view_mat, output.position);
    output.meshlet_id = meshlet_instance.meshlet_index;
    return output;
}

uint hash(uint a) {
    a = (a + 0x7ed55d16) + (a << 12);
    a = (a ^ 0xc761c23c) ^ (a >> 19);
    a = (a + 0x165667b1) + (a << 5);
    a = (a + 0xd3a2646c) ^ (a << 9);
    a = (a + 0xfd7046c5) + (a << 3);
    a = (a ^ 0xb55a4f09) ^ (a >> 16);
    return a;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target {
    const u32 h = hash(input.meshlet_id);
    const float3 color = vec3(float(h & 255), float((h >> 8) & 255), float((h >> 16) & 255)) / 255.0;
    return float4(color, 1.0);
}

