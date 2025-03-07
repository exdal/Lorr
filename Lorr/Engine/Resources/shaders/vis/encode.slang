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

struct VisibleMeshletInstancesIndices {
    u32 indices[];
};
[[vk::binding(4, 0)]] restrict writeonly
GLSLShaderStorageBuffer<VisibleMeshletInstancesIndices, ScalarDataLayout> visible_meshlet_instances_indices;

struct VertexOutput {
    float4 position : SV_Position;
    nointerpolation u32 meshlet_index : MESHLET_ID;
    nointerpolation u32 triangle_index : TRIANGLE_ID;
};

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    const u32 visible_meshlet_index = (vertex_id >> MESHLET_PRIMITIVE_BITS) & MESHLET_ID_MASK;
    const u32 triangle_index = vertex_id & MESHLET_PRIMITIVE_MASK;

    const u32 meshlet_instance_index = visible_meshlet_instances_indices.indices[visible_meshlet_index];
    const MeshletInstance meshlet_instance = meshlet_instances.instances[meshlet_instance_index];
    const Model model = models.models[meshlet_instance.model_index];

    const Meshlet meshlet = model.meshlets[meshlet_instance.meshlet_index];
    const u32 vertex_offset = meshlet.vertex_offset;
    const u32 index_offset = meshlet.index_offset;
    const u32 triangle_offset = meshlet.triangle_offset;

    const u32 local_triangle = u32(model.local_triangle_indices[triangle_offset + triangle_index]);
    const u32 index = model.indices[index_offset + local_triangle];
    const float3 vertex_pos = model.vertex_positions[vertex_offset + index];
    const Transform transform = transforms.transforms[meshlet_instance.transform_index];

    VertexOutput output;
    output.position = mul(transform.local, float4(vertex_pos, 1.0));
    output.position = mul(scene.camera.projection_view_mat, output.position);
    output.meshlet_index = visible_meshlet_index;
    output.triangle_index = triangle_index / 3;
    return output;
}

[shader("fragment")]
u32 fs_main(VertexOutput input) : SV_Target {
    return (input.meshlet_index << MESHLET_PRIMITIVE_BITS) | (input.triangle_index & MESHLET_PRIMITIVE_MASK);
}

