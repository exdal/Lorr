module model;

import lorr;

[[vk::push_constant]] struct {
    World *world;
    u32 model_transform_index;
    u32 material_id;
} c;

struct VertexInput {
    float3 position  : POSITION;
    float uv_x       : UV_X;
    float3 normal    : NORMAL;
    float uv_y       : UV_Y;
};

struct VertexOutput {
    float4 position : SV_Position;
    float3 normal;
    float2 tex_coord;
};

[shader("vertex")]
VertexOutput vs_main(VertexInput input) {
    const let world = c.world;
    const let camera = world.cameras[world.active_camera_index];
    const let model = world.models[c.model_transform_index];

    VertexOutput output;
    output.position = mul(camera.projection_view_mat, mul(float4(input.position, 1.0), model.model_transform_mat));
    output.normal = input.normal;
    output.tex_coord = float2(input.uv_x, input.uv_y);
    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target {
    const let material = c.world->materials[c.material_id];
    const let texture = ImageView(material.albedo_image_view);

    return /* texture.sample(material.albedo_sampler, input.tex_coord) * material.albedo_color */ float4(0.0, 0.0, 0.0, 1.0);
}
