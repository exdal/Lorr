import vis;
import "../lorr";

[[vk::binding(0, 0)]] ConstantBuffer<Scene, ScalarDataLayout> scene;

[[vk::binding(1, 0)]] restrict readonly
RWStructuredBuffer<Transform, ScalarDataLayout> transforms;

[[vk::binding(2, 0)]] restrict readonly
RWStructuredBuffer<Model, ScalarDataLayout> models;

[[vk::binding(3, 0)]] restrict readonly
RWStructuredBuffer<MeshletInstance, ScalarDataLayout> meshlet_instances;

[[vk::binding(4, 0)]] restrict writeonly
RWStructuredBuffer<u32, ScalarDataLayout> visible_meshlet_instances_indices;

[[vk::binding(5, 0)]] restrict readonly
RWStructuredBuffer<Material, ScalarDataLayout> materials;

[[vk::binding(0, 1)]] Texture2D<float4> textures[];
[[vk::binding(1, 1)]] SamplerState samplers[];

struct VertexOutput {
    float4 position : SV_Position;
    float3 vertex_pos : POSITION;
    float2 tex_coord : TEXCOORD;
    nointerpolation u32 meshlet_index : MESHLET_ID;
    nointerpolation u32 triangle_index : TRIANGLE_ID;
    nointerpolation u32 material_index : MATERIAL_ID;
};

[shader("vertex")]
VertexOutput vs_main(u32 vertex_id : SV_VertexID) {
    const u32 visible_meshlet_index = (vertex_id >> MESHLET_PRIMITIVE_BITS) & MESHLET_ID_MASK;
    const u32 triangle_index = vertex_id & MESHLET_PRIMITIVE_MASK;

    const u32 meshlet_instance_index = visible_meshlet_instances_indices[visible_meshlet_index];
    const MeshletInstance meshlet_instance = meshlet_instances[meshlet_instance_index];
    const Model model = models[meshlet_instance.mesh_index];

    const Meshlet meshlet = model.meshlets[meshlet_instance.meshlet_index];
    const u32 vertex_offset = meshlet.vertex_offset;
    const u32 index_offset = meshlet.index_offset;
    const u32 triangle_offset = meshlet.triangle_offset;

    const u32 local_triangle = u32(model.local_triangle_indices[triangle_offset + triangle_index]);
    const u32 index = model.indices[index_offset + local_triangle];
    const float3 vertex_pos = model.vertex_positions[vertex_offset + index];
    const float2 tex_coord = model.texture_coords[vertex_offset + index];
    const Transform transform = transforms[meshlet_instance.transform_index];

    VertexOutput output;
    output.position = mul(transform.world, float4(vertex_pos, 1.0));
    output.position = mul(scene.camera.projection_view_mat, output.position);
    output.vertex_pos = vertex_pos;
    output.tex_coord = tex_coord;
    output.meshlet_index = visible_meshlet_index;
    output.triangle_index = triangle_index / 3;
    output.material_index = meshlet_instance.material_index;
    return output;
}

[shader("fragment")]
u32 fs_main(VertexOutput input) : SV_Target {
    const Material material = materials[input.material_index];
    if (material.albedo_image_index != ~0u) {
        const float2 grad_x = ddx(input.tex_coord);
        const float2 grad_y = ddy(input.tex_coord);
        const float  max_obj_space_deriv_len = max(length(ddx(input.vertex_pos)), length(ddy(input.vertex_pos)));
        const float  alpha = material.albedo_color.a * 
                             textures[material.albedo_image_index].SampleGrad(samplers[material.albedo_image_index], input.tex_coord, grad_x, grad_y).a;

        if (alpha < clamp(material.alpha_cutoff, 0.001, 1.0)) {
            discard;
        }
    }
    
    return (input.meshlet_index << MESHLET_PRIMITIVE_BITS) | (input.triangle_index & MESHLET_PRIMITIVE_MASK);
}

