import lorr;

struct VertexInput
{
    float2 position  : POSITION;
    float2 tex_coord : TEXCOORD;
    float4 color     : COLOR;
};

struct VertexOutput {
    float4 position     : SV_Position;
    float2 tex_coord    : TEXCOORD;
    float4 color        : COLOR;
};

[[vk::push_constant]] struct PushConstants
{
    float2 translate;
    float2 scale;
    ImageViewID image_view_id;
    SamplerID sampler_id;
} c;

[shader("vertex")]
VertexOutput vs_main(VertexInput input) {
    VertexOutput output;

    output.position = float4(input.position * c.scale + c.translate, 0.0, 1.0);
    output.tex_coord = input.tex_coord;
    output.color = input.color;

    return output;
}

[shader("fragment")]
float4 fs_main(VertexOutput input) : SV_TARGET
{
    const Texture2D font_texture = Texture2D<float4>::get(c.image_view_id);
    const SamplerState linear_sampler = SamplerState::get(c.sampler_id);

    return font_texture.Sample(linear_sampler, input.tex_coord) * input.color;
}
