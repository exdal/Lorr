module triangle_id;

import "../lorr";

[[vk::binding(0, 0)]] Camera camera;
[[vk::binding(1, 0)]] StructuredBuffer<MeshInstance> mesh_instances;
[[vk::binding(2, 0)]] StructuredBuffer<Vertex> vertices;
[[vk::binding(3, 0)]] StructuredBuffer<u32> reordered_indices;

struct PushConstants {
    u32 model_index;
};
[[vk::push_constant]] PushConstants c;

struct VertexOutput {
    float4 position  : SV_Position;
    u32 primitive_id : PRIMITIVE_ID;
};

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    VertexOutput output;

    MeshInstance mesh_instance = mesh_instances[c.model_index];
    const Vertex vertex = vertices[reordered_indices[vertex_id]];
    const let world_position = mul(float4(vertex.position, 1.0), mesh_instance.transform_mat);

    output.position = mul(camera.projection_view_mat, world_position);
    output.primitive_id = vertex_id;
    return output;
}

uint hash(uint a) {
    a = (a+0x7ed55d16) + (a<<12);
    a = (a^0xc761c23c) ^ (a>>19);
    a = (a+0x165667b1) + (a<<5);
    a = (a+0xd3a2646c) ^ (a<<9);
    a = (a+0xfd7046c5) + (a<<3);
    a = (a^0xb55a4f09) ^ (a>>16);
    return a;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target {
    uint mhash = hash(input.primitive_id);
    vec3 mcolor = vec3(float(mhash & 255), float((mhash >> 8) & 255), float((mhash >> 16) & 255)) / 255.0;
    return float4(mcolor, 1.0);
}

