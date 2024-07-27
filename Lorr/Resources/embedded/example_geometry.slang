import lorr;

[[vk::push_constant]] struct {
    float4x4 camera_mat;
    BufferID material_buffer_id;
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
    VertexOutput output;
    output.position = mul(c.camera_mat, float4(input.position, 1.0));
    output.tex_coord = float2(input.uv_x, input.uv_y);
    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_Target {
    Material material = lr::load_buffer<Material>(c.material_buffer_id)[c.material_id];

    const Texture2D texture = Texture2D<float4>::get(material.albedo_image_view);
    const SamplerState sampler = SamplerState::get(material.albedo_sampler);

    return texture.Sample(sampler, input.tex_coord);
}
