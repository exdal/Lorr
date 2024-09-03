import "lorr";

[[vk::push_constant]] struct {
    lr::World *world;
    u32 model_id;
    u32 material_id;
} c;

struct VertexInput {
    float3 position  : POSITION;
    float uv_x       : UV_X;
    float3 normal    : NORMAL;
    float uv_y       : UV_Y;
    float4 color     : COLOR;
};

struct VertexOutput {
    float4 position : SV_Position;
    float2 tex_coord;
};

[shader("vertex")]
VertexOutput vs_main(VertexInput input)
{
    lr::Camera *camera = &c.world->cameras[0];
    lr::Model *model = &c.world->models[c.model_id];

    VertexOutput output;
    output.position = mul(camera->projection_view_mat, mul(float4(input.position, 1.0), model->model_transform_mat));
    output.tex_coord = float2(input.uv_x, input.uv_y);
    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target {
    lr::Material *material = &c.world->materials[c.material_id];

    let texture = Texture2D<float4>::get(material->albedo_image_view);
    let sampler = SamplerState::get(material->albedo_sampler);

    return texture.Sample(sampler, input.tex_coord);
}
